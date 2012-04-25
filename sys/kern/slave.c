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

static char cbuf[PAGESIZE];
extern uint32_t time;

//bool as_checkrange (as_t* as, uint32_t addr, size_t size);
//uint32_t usercopy(uint32_t dest, uint32_t src, size_t size);
//uint32_t copy_from_user(uint32_t dest, uint32_t src, size_t size);

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

        pmap_copy(kern_ptab, (uintptr_t) dest, user_pmap, (uintptr_t) src, size);

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
//	cprintf("slave system call\n");
    switch (cmd) {
	case SYSCALL_CLIENT_PUTS:
	    if (copy_from_user(cbuf,&arg, PAGESIZE) == 0)
				syscall_fail(ctx);
	    cbuf[PAGESIZE-1] = 0;
	    cprintf("%s", cbuf);
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
		cprintf("slave#slave timer!\n");
		wait_to_start();
	}
	return 0;
}

uint32_t spgflt(context_t* ctx) {
	//static uint32_t prevfault=0;

	uint8_t mycpu = pcpu_cur_idx();
	uint32_t fault = rcr2();
	KERN_ASSERT (cpus[mycpu].running);
//	sig->type = SIGNAL_PGFLT;
//	((signal_pgflt*)&sig->data)->cpu = mycpu;
//	((signal_pgflt*)&sig->data)->procid = cpus[mycpu].running;
//	((signal_pgflt*)&sig->data)->fault_addr = fault;
	cprintf("slave#Signalling page fault at addr %08x\n", fault);
	//msgqueue_add((char*)sig, sizeof(sigbuf));
	//msgqueue_add((char*)sig, sizeof(sigbuf));
//	mqueue_enqueue((char*)sig, sizeof(sigbuf));

	// stop the CPU
	cpus[mycpu].running = 0;
	cpus[mycpu].stop = 0;
	wait_to_start();


//	cprintf("Slave Page Fault at %x, cpu %d, reserving new page\n", fault, mp_curcpu());
//	if (as_reserve(as_current(), PGADDR(fault), PTE_W | PTE_U | PTE_P) == NULL) {
//		 cprintf("New page can not be reserved\n");
//	}
//
//	prevfault = fault;
return 0;
}


void syscall_fail(context_t* ctx) {
        cprintf("SYSCALL ERROR\n");
        context_start(ctx);
}


void wait_to_start() {

	int mycpu = pcpu_cur_idx();
	//int i=0;
	//pid_t pid;
	KERN_ASSERT(cpus[mycpu].running == FALSE);
	cprintf("slave#:CPU %d, waiting to start\n, addr cpu = %x", mycpu, &cpus[mycpu]); 
	while(cpus[mycpu].start == 0);
	cprintf("slave#CPU %d, starting process %d\n", mycpu, cpus[mycpu].start);
//	cprintf("cpustacks@%x, esp:@%x\n",pcpu_stacks[mycpu],read_esp());
	cpus[mycpu].running = cpus[mycpu].start;
	cpus[mycpu].start=0;
	proc_lock(cpus[mycpu].running);
	proc_start(cpus[mycpu].running);
}

void slave_kernel() {
	int mycpu;
	mycpu = pcpu_cur_idx();
	cprintf("slave#* current cpu is : %d\n",mycpu); 
	intr_enable(IRQ_TIMER, mycpu);
	context_register_handler(T_IRQ0+IRQ_TIMER,&stimer);
	context_register_handler(T_CLIENT_SYSCALL,&sl_syscall);
	context_register_handler(T_PGFLT,&spgflt);
//	as_init();
	// enable_amd_svm();
	cprintf("slave#I am alive on cpu number %d!!!\n", mycpu);
	wait_to_start();
}
