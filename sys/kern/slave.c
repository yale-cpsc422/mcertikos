/* See COPYRIGHT for copyright information. */
#include <sys/context.h>
#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/intr.h>
#include <sys/mgmt.h>
#include <sys/mmu.h>
#include <sys/msg.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/slave.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/vm.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>

#include <machine/pmap.h>

static void wait_to_start(void);

static uint8_t cbuf[PAGESIZE];

/* static char sigbuf[PAGESIZE-12]; */
/* static sig_t * sig = (sig_t*)&sigbuf; */

/*
 * Copy data from user's virtual address space to kernel's virtual address
 * space.
 *
 * @param dest kernel's virtual address
 * @param src user's virtual address
 * @param size the size of the data
 *
 * @return the kernel's virtual address if copy succeed; otherwise, NULL.
 */
static void *
copy_from_user(void *dest, void *src, size_t size)
{
	/* KERN_DEBUG("copy_from_user(): %x <- %x, size=%x\n", dest, src, size); */

	if (dest == NULL || src == NULL || size == 0)
		return NULL;

	pcpu_t *c;
	pid_t user_proc;
	pmap_t *user_pmap;

	c = pcpu_cur();

	spinlock_acquire(&c->lk);
	user_proc = c->proc;
	spinlock_release(&c->lk);
	KERN_ASSERT(user_proc != 0);

	proc_lock(user_proc);

	user_pmap = proc_pmap(user_proc);
	KERN_ASSERT(user_pmap == (pmap_t *) rcr3());

	if (pmap_checkrange(user_pmap, (uintptr_t) src, size) == FALSE) {
		KERN_DEBUG("%x ~ %x do not fit in the user address space.\n",
			   src, src+size);
		proc_unlock(user_proc);
		return NULL;
	}

	memcpy(dest, src, size);

	proc_unlock(user_proc);

	return dest;
}

static void
syscall_fail(context_t* ctx)
{
	KERN_DEBUG("SYSCALL ERROR\n");
	context_start(ctx);
}

static uint32_t
sl_syscall(context_t* ctx)
{
	uint32_t cmd = context_arg1(ctx);
	uint32_t arg = context_arg2(ctx);
	// uint32_t arg2 = context_arg3(ctx);
	// uint32_t arg3 = context_arg4(ctx);
	switch (cmd) {
	case SYSCALL_CLIENT_PUTS:
		if (copy_from_user(cbuf,(void * )arg, PAGESIZE) == 0)
			syscall_fail(ctx);
		cbuf[PAGESIZE-1] = 0;
		cprintf("%s", cbuf);
		memset(cbuf, 0, sizeof(char));
		break;
	case SYSCALL_CLIENT_GETC:
		//		if (!as_checkrange(as_current(), arg, sizeof(uint32_t)))
		//			syscall_fail(ctx);
		*(uint32_t*)arg = getchar();
		break;
	/* case SYSCALL_CLIENT_TIME: */
	/* 	//		if (!as_checkrange(as_current(), arg, sizeof(uint32_t))) */
	/* 	//			syscall_fail(ctx); */
	/* 	*(uint32_t*)arg = time; */
	/* 	//cprintf("slave system call time %d\n", time); */
	/* 	break; */
	case SYSCALL_CLIENT_PID:
		//		if (!as_checkrange(as_current(), arg, sizeof(uint32_t)))
		//			syscall_fail(ctx);
		;
		pcpu_t *c = pcpu_cur();
		spinlock_acquire(&c->lk);
		*(pid_t *) arg = c->proc;
		spinlock_release(&c->lk);
		break;
	case SYSCALL_CLIENT_CPU:
		//	if (!as_checkrange(as_current(), arg, sizeof(uint32_t)))
		//		syscall_fail(ctx);
		*(uint32_t*)arg = pcpu_cur_idx();
		break;
	case SYSCALL_CLIENT_SETUPVM:
		/*
		 * Setup and start a VM.
		 */
		;
		vmm_init_on_ap();
		struct vm *vm = vmm_init_vm();
		if (vm == NULL) {
			KERN_DEBUG("SYSCALL_SETUPVM: Cannot initialize a VM.\n");
			return 1;
		}
		KERN_DEBUG("SYSCALL_CLIENT_SETUPVM\n");
		vmm_run_vm(vm);

		KERN_DEBUG("SYSCALL_SETUPVM: ended.\n");

		break;

	}
	return 0;
}

static uint32_t
stimer(context_t* ctx)
{
	pcpu_t *c = pcpu_cur();

	spinlock_acquire(&c->lk);

	if (c->stat == PCPU_STOPPING) {
		KERN_ASSERT(c->proc != 0);

		pid_t proc = c->proc;

		proc_lock(proc);
		proc_stop(proc);
		proc_unlock(proc);

		c->proc = 0;
		spinlock_release(&c->lk);

		intr_eoi();

		wait_to_start();
	}

	spinlock_release(&c->lk);

	intr_eoi();

	return 0;
}

static uint32_t
spgflt(context_t* ctx)
{
	pcpu_t *c = pcpu_cur();
	uintptr_t fault = rcr2();

	/*	sig->type = SIGNAL_PGFLT;
		((signal_pgflt_t*)&sig->data)->cpu = mycpu;
		((signal_pgflt_t*)&sig->data)->procid = cpus[mycpu].running;
		((signal_pgflt_t*)&sig->data)->fault_addr = fault;
		cprintf("slave %d# Signalling page fault at addr %08x\n", mycpu, fault);
		//msgqueue_add((char*)sig, sizeof(sigbuf));
		//msgqueue_add((char*)sig, sizeof(sigbuf));
		mqueue_enqueue((char*)sig, sizeof(sigbuf));

	*/	// stop the CPU
	/*	cpus[mycpu].running = 0;
		cpus[mycpu].stop = 0;
		wait_to_start();
	*/

	pid_t user_proc;
	pmap_t *user_pmap;

	spinlock_acquire(&c->lk);
	KERN_ASSERT(c->stat == PCPU_RUNNING);
	user_proc = c->proc;
	spinlock_release(&c->lk);
	KERN_ASSERT(user_proc != 0);

	proc_lock(user_proc);

	user_pmap = proc_pmap(user_proc);
	KERN_ASSERT(user_pmap == (pmap_t *) rcr3());

	if (!pmap_reserve(user_pmap, (uintptr_t) PGADDR(fault),
			  PTE_W | PTE_U | PTE_P)) {
		KERN_DEBUG("Cannot allocate physical memory for 0x%x\n",
			   fault);
		KERN_PANIC("Stop here.\n");
		return 1;
	}

	proc_unlock(user_proc);

	return 0;
}

static void
wait_to_start(void)
{
	pcpu_t *c;
	pid_t proc;

	c = pcpu_cur();
	spinlock_acquire(&c->lk);
	KERN_ASSERT(c->stat == PCPU_WAIT);
	spinlock_release(&c->lk);

	KERN_DEBUG("slave %d# waiting to start\n", pcpu_cur_idx());
	/* busy waiting for a process */
	while (1) {
		spinlock_acquire(&c->lk);
		if (c->stat == PCPU_READY)
			break;
		spinlock_release(&c->lk);
		pause();
	}

	proc = c->proc;
	KERN_ASSERT(proc != 0);

	KERN_DEBUG("slave %d# starting process %d\n", pcpu_cur_idx(), proc);

	/* start the process */
	proc_start(proc);
}

void
slave_kernel()
{
	int mycpu;
	mycpu = pcpu_cur_idx();
	KERN_DEBUG("slave %d# current cpu is : %d\n",mycpu, mycpu);
	intr_enable(IRQ_TIMER, mycpu);
	context_register_handler(T_IRQ0+IRQ_TIMER,&stimer);
	context_register_handler(T_CLIENT_SYSCALL,&sl_syscall);
	context_register_handler(T_PGFLT,&spgflt);
	KERN_DEBUG("slave %d# I am alive on cpu number %d!!!\n", mycpu, mycpu);
	wait_to_start();
}
