#include <sys/context.h>
#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/string.h>
#include <sys/syscall.h>
#include <sys/trap.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/hvm.h>

#include <machine/pmap.h>
#include <machine/vm.h>

#include <dev/kbd.h>
#include <dev/lapic.h>
#include <dev/tsc.h>

static gcc_inline void
default_handler_user(tf_t *tf)
{
	KERN_WARN("No handler for user trap 0x%x, process %d, eip 0x%08x.\n",
		  tf->trapno, proc_cur()->pid, tf->eip);
	if (tf->trapno >= T_IRQ0)
		intr_eoi();
}

gcc_noreturn void
trap(tf_t *tf)
{
	asm volatile("cld" ::: "cc");

	KERN_ASSERT(tf != NULL);
	KERN_ASSERT(VM_USERLO <= tf->eip && tf->eip < VM_USERHI);

	trap_cb_t f;
	struct proc *cur_p = proc_cur();
	struct pcpu *cur_c = pcpu_cur();

	KERN_ASSERT(cur_p);

	proc_save(cur_p, tf);
	pmap_install(pmap_kern_map());

	f = (*cur_c->trap_handler)[tf->trapno];

	if (f)
		f(tf->trapno, &cur_p->uctx);
	else
		default_handler_user(tf);

	ctx_start(&cur_p->uctx);
}

void
trap_init_array(struct pcpu *c)
{
	KERN_ASSERT(c != NULL);
	KERN_ASSERT(c->inited == TRUE);

	int npages;
	pageinfo_t *pi;

	npages = ROUNDUP(sizeof(trap_cb_t) * T_MAX, PAGESIZE) / PAGESIZE;
	if ((pi = mem_pages_alloc(npages)) == NULL)
		KERN_PANIC("Cannot allocate memory for trap handlers.\n");

	spinlock_acquire(&c->lk);
	c->trap_handler = (trap_cb_t **) mem_pi2phys(pi);
	memzero(c->trap_handler, npages * PAGESIZE);
	spinlock_release(&c->lk);
}

void
trap_handler_register(int trapno, trap_cb_t cb)
{
	KERN_ASSERT(0 <= trapno && trapno < T_MAX);
	KERN_ASSERT(cb != NULL);

	(*pcpu_cur()->trap_handler)[trapno] = cb;
}

int
default_exception_handler(uint8_t trapno, struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);
	KERN_ASSERT(proc_cur() != NULL);

	KERN_DEBUG("Exception %d caused by process %d on CPU %d.\n",
		   ctx->tf.trapno, proc_cur()->pid, pcpu_cpu_idx(pcpu_cur()));
	ctx_dump(ctx);

	KERN_PANIC("Stop here.\n");

	return 0;
}

int
gpf_handler(uint8_t trapno, struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);
	KERN_ASSERT(proc_cur() != NULL);

	KERN_DEBUG("General protection fault caused by process %d on CPU %d.\n",
		   proc_cur()->pid, pcpu_cpu_idx(pcpu_cur()));
	ctx_dump(ctx);

	/*
	 * TODO: kill the process which caused the fault instead of kernel panic
	 */
	KERN_PANIC("Stop here.\n");

	return 0;
}

int
pgf_handler(uint8_t trapno, struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);

	struct proc *p = proc_cur();
	KERN_ASSERT(p != NULL);

	uint32_t errno = ctx_errno(ctx);
	uintptr_t fault_va = rcr2();

	/* KERN_DEBUG("Page fault at 0x%08x caused by process %d on CPU %d.\n", */
	/* 	   fault_va, p->pid, pcpu_cpu_idx(pcpu_cur())); */
	/* trap_dump(&ctx->tf); */

	if (errno & PFE_PR) {
		/*
		 * TODO: kill the process instead of kernel panic
		 */
		trap_dump(&ctx->tf);
		KERN_PANIC("Page fault caused for permission violation "
			   "(va 0x%08x).\n", fault_va);
		return 1;
	}

	proc_lock(p);

	if (!pmap_reserve(p->pmap, (uintptr_t) PGADDR(fault_va),
			  PTE_W | PTE_U | PTE_P)) {
		/*
		 * TODO: kill the process instead of kernel panic
		 */
		KERN_PANIC("Cannot allocate physical memory for 0x%x\n",
			   fault_va);
		return 1;
	}

	proc_unlock(p);

	KERN_DEBUG("Page fault at 0x%08x, process %d on CPU %d is handled.\n",
		   fault_va, p->pid, pcpu_cpu_idx(pcpu_cur()));

	return 0;
}

int
spurious_intr_handler(uint8_t trapno, struct context *ctx)
{
	KERN_DEBUG("Ignore spurious interrupt.\n");
	/* XXX: do not send EOI for spurious interrupts */
	return 0;
}

/*
 * CertiKOS uses LAPIC timers as the sources of the timer interrupts. This
 * function only handles the timer interrupts from the processor on which
 * it's running.
 */
int
timer_intr_handler(uint8_t trapno, struct context *ctx)
{
	intr_eoi();
	sched_update();
	return 0;
}

/*
 * CertiKOS redirects the keyboard interrupts to BSP.
 */
int
kbd_intr_handler(uint8_t trapno, struct context *ctx)
{
	intr_eoi();
	/* kbd_intr(); */
	return 0;
}

/*
 * CertiKOS uses IRQ_IPI_RESCHED to force the scheduler on another processor to
 * reschedule.
 */
int
ipi_resched_handler(uint8_t trapno, struct context *ctx)
{
	intr_eoi();
	sched_lock(pcpu_cur());
	sched_resched(TRUE);
	sched_unlock(pcpu_cur());
	return 0;
}
