#include <sys/console.h>
#include <sys/context.h>
#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/mboot.h>
#include <sys/mem.h>
#include <sys/mmu.h>
#include <sys/pcpu.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/timer.h>
#include <sys/trap.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>

#include <machine/pmap.h>

#include <dev/kbd.h>
#include <dev/pci.h>
#include <dev/tsc.h>
#include <dev/timer.h>

uint8_t pcpu_stack[MAX_CPU * PAGE_SIZE] gcc_aligned(PAGE_SIZE);

extern uint8_t _binary___obj_user_init_init_start[];

static void gcc_noreturn
kern_main(void)
{
	pageinfo_t *pi;
	struct pcpu *c;
	struct proc *init_proc;

	c = pcpu_cur();
	KERN_ASSERT(c != NULL && c->state == PCPU_READY);

	/* allocate buffer for handling system calls */
	KERN_INFO("[BSP KERN] Prepare buffer for handling system calls ... ");
	if ((pi = mem_page_alloc()) == NULL)
		KERN_PANIC("Cannot allocate memory for handling system calls.\n");
	spinlock_acquire(&c->lk);
	c->sys_buf = (char *) mem_pi2phys(pi);
	spinlock_release(&c->lk);
	KERN_INFO("done.\n");

	/* register trap handlers */
	KERN_INFO("[BSP KERN] Register exception handlers ... ");
	trap_handler_register(T_GPFLT, gpf_handler);
	trap_handler_register(T_PGFLT, pgf_handler);
	trap_handler_register(T_SYSCALL, syscall_handler);
	/* use default handler to handle other exceptions */
	trap_handler_register(T_DIVIDE, default_exception_handler);
	trap_handler_register(T_DEBUG, default_exception_handler);
	trap_handler_register(T_NMI, default_exception_handler);
	trap_handler_register(T_BRKPT, default_exception_handler);
	trap_handler_register(T_OFLOW, default_exception_handler);
	trap_handler_register(T_BOUND, default_exception_handler);
	trap_handler_register(T_ILLOP, default_exception_handler);
	trap_handler_register(T_DEVICE, default_exception_handler);
	trap_handler_register(T_DBLFLT, default_exception_handler);
	trap_handler_register(T_COPROC, default_exception_handler);
	trap_handler_register(T_TSS, default_exception_handler);
	trap_handler_register(T_SEGNP, default_exception_handler);
	trap_handler_register(T_STACK, default_exception_handler);
	trap_handler_register(T_RES, default_exception_handler);
	trap_handler_register(T_FPERR, default_exception_handler);
	trap_handler_register(T_ALIGN, default_exception_handler);
	trap_handler_register(T_MCHK, default_exception_handler);
	trap_handler_register(T_SIMD, default_exception_handler);
	trap_handler_register(T_SECEV, default_exception_handler);
	KERN_INFO("done.\n");

	KERN_INFO("[BSP KERN] Register interrupt handlers ... ");
	trap_handler_register(T_IRQ0+IRQ_SPURIOUS, spurious_intr_handler);
	trap_handler_register(T_IRQ0+IRQ_TIMER, timer_intr_handler);
	trap_handler_register(T_IRQ0+IRQ_KBD, kbd_intr_handler);
	KERN_INFO("done.\n");

	/* enable interrupts */
	KERN_INFO("[BSP KERN] Enable TIMER interrupt ... ");
	intr_enable(IRQ_TIMER, 0);
	KERN_INFO("done.\n");

	KERN_INFO("[BSP KERN] Enable KBD interrupt ... ");
	kbd_intenable();
	KERN_INFO("done.\n");

	/* create init process */
	KERN_INFO("[BSP KERN] Create init process ... ");
	init_proc = proc_create((uintptr_t) _binary___obj_user_init_init_start);
	if (init_proc == NULL)
		KERN_PANIC("Cannot create init process on CPU %d.\n",
			   pcpu_cur_idx());
	if (proc_ready(init_proc, c))
		KERN_PANIC("Cannot put init process %d on the ready queue "
			   "of CPU %d.\n", init_proc->pid, pcpu_cur_idx());
	KERN_INFO("done.\n");

	/* goto userspace */
	KERN_INFO("[BSP KERN] Start init process.\n");
	c->state = PCPU_RUNNING;
	proc_sched(c);
	proc_run();
}

static void
kern_main_ap(void)
{
	pageinfo_t *pi;
	struct pcpu *c;
	struct proc *init_proc;
	int cpu_idx = pcpu_cur_idx();

	c = pcpu_cur();
	KERN_ASSERT(c != NULL && c->state == PCPU_READY);

	/* allocate buffer for handling system calls */
	KERN_INFO("[AP%d KERN] Prepare buffer for handling system calls ... ",
		  cpu_idx);
	if ((pi = mem_page_alloc()) == NULL)
		KERN_PANIC("Cannot allocate memory for handling system calls.\n");
	spinlock_acquire(&c->lk);
	c->sys_buf = (char *) mem_pi2phys(pi);
	spinlock_release(&c->lk);
	KERN_INFO("done.\n");

	/* register trap handlers */
	KERN_INFO("[AP%d KERN] Register exception handlers ... ", cpu_idx);
	trap_handler_register(T_GPFLT, gpf_handler);
	trap_handler_register(T_PGFLT, pgf_handler);
	trap_handler_register(T_SYSCALL, syscall_handler);
	/* use default handler to handle other exceptions */
	trap_handler_register(T_DIVIDE, default_exception_handler);
	trap_handler_register(T_DEBUG, default_exception_handler);
	trap_handler_register(T_NMI, default_exception_handler);
	trap_handler_register(T_BRKPT, default_exception_handler);
	trap_handler_register(T_OFLOW, default_exception_handler);
	trap_handler_register(T_BOUND, default_exception_handler);
	trap_handler_register(T_ILLOP, default_exception_handler);
	trap_handler_register(T_DEVICE, default_exception_handler);
	trap_handler_register(T_DBLFLT, default_exception_handler);
	trap_handler_register(T_COPROC, default_exception_handler);
	trap_handler_register(T_TSS, default_exception_handler);
	trap_handler_register(T_SEGNP, default_exception_handler);
	trap_handler_register(T_STACK, default_exception_handler);
	trap_handler_register(T_RES, default_exception_handler);
	trap_handler_register(T_FPERR, default_exception_handler);
	trap_handler_register(T_ALIGN, default_exception_handler);
	trap_handler_register(T_MCHK, default_exception_handler);
	trap_handler_register(T_SIMD, default_exception_handler);
	trap_handler_register(T_SECEV, default_exception_handler);
	KERN_INFO("done.\n");

	KERN_INFO("[AP%d KERN] Register interrupt handlers ... ", cpu_idx);
	trap_handler_register(T_IRQ0+IRQ_SPURIOUS, spurious_intr_handler);
	trap_handler_register(T_IRQ0+IRQ_TIMER, timer_intr_handler);
	trap_handler_register(T_IRQ0+IRQ_KBD, kbd_intr_handler);
	KERN_INFO("done.\n");

	/* enable interrupts */
	KERN_INFO("[AP%d KERN] Enable TIMER interrupt ... ", cpu_idx);
	intr_enable(IRQ_TIMER, 0);
	KERN_INFO("done.\n");

	KERN_INFO("[AP%d KERN] Enable KBD interrupt ... ", cpu_idx);
	kbd_intenable();
	KERN_INFO("done.\n");

	/* create init process */
	KERN_INFO("[AP%d KERN] Create init process ... ", cpu_idx);
	init_proc = proc_create((uintptr_t) _binary___obj_user_init_init_start);
	if (init_proc == NULL)
		KERN_PANIC("Cannot create init process on CPU %d.\n",
			   pcpu_cur_idx());
	if (proc_ready(init_proc, c))
		KERN_PANIC("Cannot put init process %d on the ready queue "
			   "of CPU %d.\n", init_proc->pid, pcpu_cur_idx());
	KERN_INFO("done.\n");

	/* goto userspace */
	KERN_INFO("[AP%d KERN] Start init process.\n", cpu_idx);
	c->state = PCPU_RUNNING;
	proc_sched(c);
	proc_run();
}

void gcc_noreturn
kern_init(mboot_info_t *mbi)
{
	/*
	 * Clear BSS.
	 *
	 *           :              :
	 *           |              |
	 *       /   +--------------+ <-- end
	 *       |   |              |
	 *       |   :      SBZ     :
	 *       |   |              |
	 *       |   +--------------+ <-- pcpu_stack + PAGE_SIZE
	 *      BSS  |              |
	 *       |   +--------------+ <-- pcpu_stack
	 *       |   |              |
	 *       |   :      SBZ     :
	 *       |   |              |
	 *       \   +--------------+ <-- edata
	 *           |              |
	 *           :              :
	 */
	extern uint8_t end[], edata[];
	uint8_t *stack = (uint8_t *) pcpu_stack;
	memset(edata, 0x0, stack - edata);
	memset(stack + PAGE_SIZE, 0x0, end - stack - PAGE_SIZE);

	/*
	 * Initialize the console so that we can output debug messages to the
	 * screen and/or the serial port.
	 */
	cons_init();
	debug_init();
	KERN_INFO("Console is ready.\n");

	/*
	 * Initialize kernel memory allocator.
	 */
	KERN_INFO("Initialize kernel memory allocator ... ");
	mem_init(mbi);
	KERN_INFO("done.\n");

	/*
	 * Initialize the bootstrap CPU.
	 */
	KERN_INFO("Initialize BSP ... ");
	pcpu_init();
	KERN_INFO("done.\n");

	/*
	 * Initialize kernel page table.
	 */
	KERN_INFO("Initialize kernel page table ... ");
	pmap_init();
	KERN_INFO("done.\n");

	/*
	 * Initialize i8253 timer.
	 * XXX: MUST be initialized before tsc_init().
	 */
	KERN_INFO("Initialize i8253 timer ... ");
	timer_hw_init();
	KERN_INFO("done.\n");

	/*
	 * Calibrate TSC.
	 * XXX: Must be initialized before lapic_init().
	 */
	KERN_INFO("Initialize TSC ... ");
	if (tsc_init()) {
		KERN_INFO("failed.\n");
		KERN_WARN("System time will be inaccurate.\n");
	}
	KERN_INFO("done.\n");

	/*
	 * Intialize interrupt system.
	 * XXX: lapic_init() is called in intr_init().
	 */
	KERN_INFO("Initialize the interrupt system ... ");
	intr_init();
	KERN_INFO("done.\n");

	/*
	 * Initialize context
	 */
	KERN_INFO("Initialize BSP context ... ");
	pcpu_cur()->state = PCPU_BOOTUP;
	pcpu_init_cpu();
	KERN_ASSERT(pcpu_cur()->state == PCPU_READY);
	KERN_INFO("done.\n");

	/*
	 * Initialize virtual machine monitor module.
	 */
	KERN_INFO("Initialize VMM ... ");
	if (vmm_init() != 0)
		KERN_INFO("failed.\n");
	else
		KERN_INFO("done.\n");

	/* Initialize timer */
	KERN_INFO("Initialize timer event list ... ");
	timer_init();
	KERN_INFO("done.\n");

	/* Initialize PCI bus */
	KERN_INFO("Initialize PCI ... \n");
	pci_init();
	KERN_INFO("done.\n");

	/* Initialize process module. */
	KERN_INFO("Initialize process module ... ");
	proc_init();
	KERN_INFO("done.\n");

	/* Start slave kernel on APs */
	int i;
	for (i = 1; i < pcpu_ncpu(); i++) {
		KERN_INFO("Start slave kernel on CPU%d ... ", i);
		pcpu_boot_ap(i, kern_main_ap, (uintptr_t) &stack[i * PAGE_SIZE]);
		KERN_INFO("done.\n");
	}

	/* Start master kernel on BSP */
	KERN_INFO("Start kernel on BSP ... \n");
	kern_main();

	/* should not be here */
	KERN_PANIC("We should not be here.\n");
}

void
kern_init_ap(void (*f)(void))
{
	struct pcpu *c = pcpu_cur();

	KERN_ASSERT(c->state == PCPU_SHUTDOWN);

	c->state = PCPU_BOOTUP;

	pcpu_init_cpu();
	intr_init();
	pmap_init();
	f();
}
