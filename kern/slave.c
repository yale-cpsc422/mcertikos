/* See COPYRIGHT for copyright information. */

#include <inc/gcc.h>
#include <architecture/types.h>
#include <architecture/x86.h>
#include <architecture/mem.h>
#include <architecture/mmu.h>
#include <inc/elf.h>
#include <architecture/mp.h>
#include <architecture/context.h>

#include <kern/debug/debug.h>
#include <kern/debug/stdio.h>
#include <kern/mem/mem.h>
#include <kern/as/as.h>
#include <kern/as/vm.h>

#include <kern/msgqueue/msgqueue.h>

#include <kern/kernel.h>
#include <inc/client.h>
#include <inc/user.h>

#include <kern/hvm/svm/vm.h>

#define T_SYSCALL 48

extern volatile cpu_use cpus[];
void wait_to_start(void);

static char cbuf[PAGESIZE];
extern uint32_t time;

bool as_checkrange (as_t* as, uint32_t addr, size_t size);
uint32_t usercopy(uint32_t dest, uint32_t src, size_t size);
void syscall_fail(context* ctx);

char sigbuf[PAGESIZE-12];
signal* sig = (signal*)&sigbuf;

uint32_t sl_syscall(context* ctx) {
    uint32_t cmd = context_arg1(ctx);
    uint32_t arg = context_arg2(ctx);
    uint32_t arg2 = context_arg3(ctx);
    uint32_t arg3 = context_arg4(ctx);
//	cprintf("slave system call\n");
    switch (cmd) {
	case SYSCALL_CLIENT_PUTS:
	    if (usercopy((uint32_t)cbuf,arg, PAGESIZE) == 0)
				syscall_fail(ctx);
	    cbuf[PAGESIZE-1] = 0;
	    cprintf("%s", cbuf);
	    break;
		case SYSCALL_CLIENT_GETC:
			if (!as_checkrange(as_current(), arg, sizeof(uint32_t)))
				syscall_fail(ctx);
			*(uint32_t*)arg = getchar();
			break;
		case SYSCALL_CLIENT_TIME:
			if (!as_checkrange(as_current(), arg, sizeof(uint32_t)))
				syscall_fail(ctx);
			*(uint32_t*)arg = time;
			break;
		case SYSCALL_CLIENT_PID:
			if (!as_checkrange(as_current(), arg, sizeof(uint32_t)))
				syscall_fail(ctx);
			*(uint32_t*)arg = cpus[mp_curcpu()].running;
			break;
		case SYSCALL_CLIENT_CPU:
			if (!as_checkrange(as_current(), arg, sizeof(uint32_t)))
				syscall_fail(ctx);
			*(uint32_t*)arg = mp_curcpu();
			break;
		case SYSCALL_CLIENT_SETUPVM:
			 if (!as_checkrange(as_current(), arg, sizeof(uint32_t)))
				syscall_fail(ctx);
				start_vm_with_interception();
			break;

    }
    return 0;
}

uint32_t stimer(context* ctx) {
	interrupts_eoi();
	int mycpu = mp_curcpu();
	if (cpus[mycpu].running && cpus[mycpu].stop) {
		cpus[mycpu].running = 0;
		cpus[mycpu].stop = 0;
		// should switch away address space I would think....
		wait_to_start();
	}
	return 0;
}

uint32_t spgflt(context* ctx) {
	static uint32_t prevfault=0;

	uint8_t mycpu = mp_curcpu();
	uint32_t fault = rcr2();
	assert (cpus[mycpu].running);
	sig->type = SIGNAL_PGFLT;
	((signal_pgflt*)&sig->data)->cpu = mycpu;
	((signal_pgflt*)&sig->data)->procid = cpus[mycpu].running;
	((signal_pgflt*)&sig->data)->fault_addr = fault;
	cprintf("Signalling page fault at addr %08x\n", fault);
	msgqueue_add((char*)sig, sizeof(sigbuf));

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

void wait_to_start() {

	int mycpu = mp_curcpu();
	int i=0;
	procid_t pid;
	assert(cpus[mycpu].running == false);
	cprintf("CPU %d, waiting to start\n, addr cpu = %x",
		mycpu, &cpus[mycpu]);
	while(cpus[mycpu].start == 0);
	cprintf("CPU %d, starting process %d\n", mycpu, cpus[mycpu].start);
//	cprintf("cpustacks@%x, esp:@%x\n",cpu_stacks[mycpu],read_esp())
	cpus[mycpu].running = cpus[mycpu].start;
	cpus[mycpu].start=0;
	proc_start(cpus[mycpu].running);
}

void slave_kernel() {
	int mycpu;
	mycpu = mp_curcpu();
	cprintf("* current cpu is : %d\n",mycpu);
	interrupts_enable(IRQ_TIMER, mycpu);
	context_handler(T_IRQ0+IRQ_TIMER,&stimer);
	context_handler(T_CLIENT_SYSCALL,&sl_syscall);
	context_handler(T_PGFLT,&spgflt);
	as_init();
	// enable_amd_svm();
	cprintf("I am alive on cpu number %d!!!\n", mycpu);
	wait_to_start();
}
