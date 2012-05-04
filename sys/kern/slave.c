/* See COPYRIGHT for copyright information. */
#include <sys/gcc.h>
#include <sys/debug.h>
#include <sys/slave.h>
#include <sys/types.h>
#include <sys/x86.h>
#include <sys/context.h>
#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/mgmt.h>
#include <sys/mmu.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/vm.h>
#include <sys/slave.h>

#include <sys/virt/vmm.h>

#include <sys/msg.h>

#include <machine/pmap.h>

//#include <dev/kbd.h>

#define T_SYSCALL 48

extern volatile cpu_use cpus[];
void wait_to_start(void);

static uint8_t cbuf[PAGESIZE];
extern uint32_t time;


char sigbuf[PAGESIZE-12];
sig_t * sig = (sig_t*)&sigbuf;

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

        pmap_t *user_pmap = pcpu_cur()->proc->pmap;
        if (pmap_checkrange(user_pmap, (uintptr_t) src, size) == FALSE) {
                KERN_DEBUG("%x ~ %x do not fit in the user address space.\n",
                           src, src+size);
                return NULL;
        }

	memcpy(dest, src, size);

        return dest;
}


void syscall_fail(context_t* ctx);

//char sigbuf[PAGESIZE-12];
//signal* sig = (signal*)&sigbuf;

uint32_t sl_syscall(context_t* ctx) {
    uint32_t cmd = context_arg1(ctx);
    uint32_t arg = context_arg2(ctx);
   // uint32_t arg2 = context_arg3(ctx);
   // uint32_t arg3 = context_arg4(ctx);
    switch (cmd) {
		case SYSCALL_CLIENT_PUTS:
	    		if (copy_from_user(cbuf,(void * )arg, PAGESIZE) == 0)
				syscall_fail(ctx);
	    		cbuf[PAGESIZE-1] = 0;
	    		cprintf("%s\n", cbuf);
			memset(cbuf, 0, sizeof(char));
	    		break;
		case SYSCALL_CLIENT_GETC:
	//		if (!as_checkrange(as_current(), arg, sizeof(uint32_t)))
	//			syscall_fail(ctx);
			*(uint32_t*)arg = getchar();
			break;
		case SYSCALL_CLIENT_TIME:
	//		if (!as_checkrange(as_current(), arg, sizeof(uint32_t)))
	//			syscall_fail(ctx);
			*(uint32_t*)arg = time;
			//cprintf("slave system call time %d\n", time);
			break;
		case SYSCALL_CLIENT_PID:
	//		if (!as_checkrange(as_current(), arg, sizeof(uint32_t)))
	//			syscall_fail(ctx);
			*(uint32_t*)arg = cpus[pcpu_cur_idx()].running;
			break;
		case SYSCALL_CLIENT_CPU:
		//	if (!as_checkrange(as_current(), arg, sizeof(uint32_t)))
		//		syscall_fail(ctx);
			*(uint32_t*)arg = pcpu_cur_idx();
			break;
		case SYSCALL_CLIENT_SETUPVM:
		//	 if (!as_checkrange(as_current(), arg, sizeof(uint32_t)))
		//		syscall_fail(ctx);
		//		start_vm_with_interception();
			break;

    }
    return 0;
}

uint32_t stimer(context_t* ctx) {
	intr_eoi();
	int mycpu = pcpu_cur_idx();
	if (cpus[mycpu].running && cpus[mycpu].stop) {
		cpus[mycpu].running = 0;
		cpus[mycpu].stop = 0;
		// should switch away address space I would think....
		wait_to_start();
	}
	return 0;
}

uint32_t spgflt(context_t* ctx) {

	uint8_t mycpu = pcpu_cur_idx();
	uint32_t fault = rcr2();
	KERN_ASSERT (cpus[mycpu].running);
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
	pmap_t *user_pmap = pcpu_cur()->proc->pmap;

        if (!pmap_reserve(user_pmap, (uintptr_t) PGADDR(fault),
                          PTE_W | PTE_U | PTE_P)) {
                KERN_DEBUG("Cannot allocate physical memory for 0x%x\n",
                           fault);
                KERN_PANIC("Stop here.\n");
                return 1;
        }

	return 0;
}


void syscall_fail(context_t* ctx) {
        cprintf("SYSCALL ERROR\n");
        context_start(ctx);
}


void wait_to_start() {

	int mycpu = pcpu_cur_idx();
	KERN_ASSERT(cpus[mycpu].running == FALSE);
	cprintf("slave %d# waiting to start\n", mycpu); 
	pcpu_cur()->stat=PCPU_STOP;
	while(cpus[mycpu].start == 0);
	cprintf("slave %d# starting process %d\n", mycpu, cpus[mycpu].start);
	cpus[mycpu].running = cpus[mycpu].start;
	cpus[mycpu].start=0;
	proc_lock(cpus[mycpu].running);
	proc_start(cpus[mycpu].running);
}

void slave_kernel() {
	int mycpu;
	mycpu = pcpu_cur_idx();
	cprintf("slave %d# current cpu is : %d\n",mycpu, mycpu); 
	intr_enable(IRQ_TIMER, mycpu);
	context_register_handler(T_IRQ0+IRQ_TIMER,&stimer);
	context_register_handler(T_CLIENT_SYSCALL,&sl_syscall);
	context_register_handler(T_PGFLT,&spgflt);
	cprintf("slave %d# I am alive on cpu number %d!!!\n", mycpu, mycpu);
	wait_to_start();
}
