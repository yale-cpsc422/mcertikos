/* See COPYRIGHT for copyright information. */

#include <inc/arch/gcc.h>
#include <inc/arch/types.h>
#include <inc/arch/x86.h>
#include <inc/arch/mem.h>
#include <inc/arch/mmu.h>
#include <inc/arch/elf.h>
#include <inc/arch/mp.h>
#include <inc/arch/context.h>

#include <kern/debug/debug.h>
#include <kern/debug/stdio.h>
#include <kern/mem/mem.h>
#include <kern/as/as.h>
#include <kern/as/vm.h>
#include <kern/proc/proc.h>
#include <kern/msgqueue/msgqueue.h>

#include <inc/user.h>
#include <kern/master.h>
#include <kern/slave.h>
#include <kern/kernel.h>

// Called first from entry.S on the bootstrap processor,
// and later from boot/bootother.S on all other processors.
// As a rule, "init" functions in PIOS are called once on EACH processor.

#define ROOTEXE_START _binary_obj_user_mgmt_start
extern char ROOTEXE_START[];
procid_t mgmt;

volatile kstack stacks[MAX_CPU];
volatile cpu_use cpus[MAX_CPU];

uint32_t time=0;


// SYSCALL handlers
uint32_t pgflt(context* ctx) {
    static uint32_t prevfault=0;
		  uint32_t fault = rcr2();
		  cprintf("Page Fault at %x, reserving new page\n", fault);
		  proc_debug(mgmt);
		  // ONLY MGMT APP SHOULD EVER PF
		  assert (mgmt);
		  assert(as_current() == proc_as(mgmt));
		  if (as_reserve(as_current(), PGADDR(fault), PTE_W | PTE_U | PTE_P) == NULL) {
					 cprintf("New page can not be reserved\n");
		  }
          // if PF occurs twice on the same address, spin
          if (fault == prevfault) while(1);
          prevfault = fault;
		  return 0;
}

uint32_t gpf(context* ctx) {
	uint32_t error = context_errno(ctx);
    static int gpfp = 0;
    if (!gpfp) {
        context_debug(ctx);
        gpfp = 1;
    }
    return 0;
    cprintf("General Protection Fault: ");
    if (error & PFE_U) cprintf("(user) ");
    if (error & PFE_PR)
        cprintf("Protection violation");
    if (error & PFE_WR)
        cprintf(" (write)\n");
    else
        cprintf("\n");
    return 0;
}

static char sysbuf[PAGESIZE];


uint32_t usercopy(uint32_t dest, uint32_t src, size_t size)
{
    if (!as_checkrange (as_current(), src, size)) 
        return 0;

    memmove((void*)dest, (void*)src, size);
    return size;
}


void syscall_fail(context* ctx) {
	cprintf("SYSCALL ERROR\n");
	context_start(ctx);
}

uint32_t do_mgmt_start(context* ctx, mgmt_start* param) {
	if (!(0 <= param->cpu && param->cpu < mp_ncpu())) {
		cprintf("CPUSTART: cpu not within range\n");
		syscall_fail(ctx);
	}
	if (!proc_idvalid(param->procid)) {
		cprintf("CPUSTART: process id not valid\n");
		syscall_fail(ctx);
	}
	if (cpus[param->cpu].start || cpus[param->cpu].stop) {
		cprintf("CPUSTART: processor %d is already busy\n", param->cpu);
		syscall_fail(ctx);
	}
	//cprintf("SYSCALL: setting cpu %d to start %d, cpus=%x\n", arg, arg2, &cpus[1]);
	//proc_debug(arg2);
	cpus[param->cpu].start = param->procid;
	return 0;
}

uint32_t do_mgmt_stop(context* ctx, mgmt_stop* param) {
	if (!(0 <= param->cpu && param->cpu < mp_ncpu()))
		syscall_fail(ctx);
	if (cpus[param->cpu].running == 0)
		syscall_fail(ctx);
	cpus[param->cpu].stop = true;
	return 0;
}

uint32_t do_mgmt_allocpage(context* ctx, mgmt_allocpage* param) {
	if (!proc_idvalid(param->procid)) {
		cprintf("MGMT_ALLOCPAGE: process id not valid\n");
		syscall_fail(ctx);
	}
	if (!(param->va >= mem_max && param->va < 0xf0000000)) {
		cprintf("MGMT_ALLOCPAGE: invalid request for addr %08x\n", param->va);
		syscall_fail(ctx);
	}
	as_reserve(proc_as(param->procid), param->va, PTE_P | PTE_U | PTE_W); 
	return 0;
}

uint32_t syscall(context* ctx) {
	uint32_t error = context_errno(ctx);
    uint32_t cmd = context_arg1(ctx);
    uint32_t arg = context_arg2(ctx);
    uint32_t arg2 = context_arg3(ctx);
    uint32_t arg3 = context_arg4(ctx);
    switch (cmd) {
        case SYSCALL_PUTS:
            if (usercopy((uint32_t)sysbuf,arg, PAGESIZE) == 0)
				syscall_fail(ctx);
            sysbuf[PAGESIZE-1] = 0;
            cprintf("%s", sysbuf);
            break;
		case SYSCALL_GETC:
			if (!as_checkrange(as_current(), arg, sizeof(uint32_t)))
				syscall_fail(ctx);
			*(uint32_t*)arg = getchar();
			break;
		case SYSCALL_NCPU:
			if (!as_checkrange(as_current(), arg, sizeof(uint32_t)))
				syscall_fail(ctx);
			*(uint32_t*)arg = mp_ncpu();
			break;
		case SYSCALL_CPUSTATUS:
			if (!as_checkrange(as_current(), arg, sizeof(uint32_t)))
				syscall_fail(ctx);
			int cpu = *(uint32_t*)arg;
			// cprintf("CPUSTATUS %d\n", cpu);
			if (!(0 <= cpu && cpu < mp_ncpu()))
				syscall_fail(ctx);
			*(uint32_t*)arg = cpus[cpu].running;
			break;
		case SYSCALL_SIGNAL:
			if (!as_checkrange(as_current(), arg, sizeof(signaldesc)))
				syscall_fail(ctx);
			proc_setsignal(mgmt,(signaldesc*)arg);
			break;
		case SYSCALL_SIGNALRET:
			if (!proc_insignal(mgmt))
				syscall_fail(ctx);
			proc_signalret(mgmt);
			break;
		case SYSCALL_LOAD:
			// copy the binary into kernel space
			// create and activate new address space
			// load the code
			// return a process id: handle to the address space + context
			
			// arg = string, arg2 = resulting procid
			if(!as_checkrange(as_current(), arg2, sizeof(uint32_t)))
				syscall_fail(ctx);
			procid_t proc = proc_new((char*)arg);
			if (!proc) {
				cprintf("Process loading failed\n");
				syscall_fail(ctx);
			}
			*(uint32_t*)arg2 = proc;
			break;
		case SYSCALL_MGMT:
			if (!as_checkrange(as_current(), arg, 4)) {
				syscall_fail(ctx);
			}
			mgmt_data* data = (mgmt_data*) arg;
			switch (data->command) {
				case MGMT_START:
					if (!as_checkrange(as_current(), data->command, sizeof(mgmt_start)))
						syscall_fail(ctx);
					do_mgmt_start(ctx, (mgmt_start*)(&data->params));
					break;

				case MGMT_STOP:
					if (!as_checkrange(as_current(), data->command, sizeof(mgmt_stop)))
						syscall_fail(ctx);
					do_mgmt_stop(ctx, (mgmt_stop*)(&data->params));
					break;
				case MGMT_ALLOCPAGE:
					if (!as_checkrange(as_current(), data->command, sizeof(mgmt_allocpage)))
						syscall_fail(ctx);
					do_mgmt_allocpage(ctx, (mgmt_allocpage*)(&data->params));
					break;
				default:
					cprintf("Unknown MGMT syscall\n"); 
					break;
			}
    }
    return 0;
}


char msgbuffer[PAGESIZE];
signal timer_sig = {SIGNAL_TIMER, ""};

uint32_t timer(context* ctx) {
	//cprintf("Timer fired!");
	size_t sz;
	time ++;
	static int counter=0;
	interrupts_eoi();
	counter ++;
	if (counter == 100) {
		counter = 0;
		//cprintf("adding msg\n");
		msgqueue_add((char*)&timer_sig, sizeof(timer_sig));
	}

	if (!proc_insignal(mgmt)) { // Is management task ready for events
		sz = msgqueue_get(msgbuffer, PAGESIZE);
		if (sz) { // we have a message to send
			//cprintf("removed msg\n");
			proc_sendsignal(mgmt,msgbuffer,sz);
		}
	}
	return 0;
}


void
init(void)
{
	int i;
	cprintf("Let's try to start those other cpus\n");
	for (i=1; i<mp_ncpu(); i++)
		mp_boot(i,slave_kernel,(uint32_t)&stacks[i]);
	cprintf("And I am back!\n");

	msgqueue_init();

	interrupts_enable(IRQ_TIMER,0);
	context_handler(T_GPFLT,&gpf);
	context_handler(T_PGFLT,&pgflt);
	context_handler(T_SYSCALL,&syscall);
	context_handler(T_IRQ0+IRQ_TIMER,&timer);
	cprintf("Verified Kernel booting\n");
    cprintf("Loading Address spaces....");
	as_init();
	if (!as_current())
		panic("Could not initialize address space!\n");
	cprintf("ok\n");


	cpus[0].running = true;
	for (i=1; i<MAX_CPU; i++)
		cpus[i].running = false;
	

	mgmt = proc_new(ROOTEXE_START);
	cprintf("Jumping to user mode\n");
    proc_start(mgmt);
	cprintf("UHOH!\n");
}

