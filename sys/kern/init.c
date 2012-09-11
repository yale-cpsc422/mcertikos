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

#include <machine/kstack.h>
#include <machine/pmap.h>

#include <dev/kbd.h>
#include <dev/lapic.h>
#include <dev/pci.h>
#include <dev/tsc.h>
#include <dev/timer.h>

/*
 * Reserve memory for the bootstrap kernel stack on the boostrap processor core.
 */
uint8_t bsp_kstack[KSTACK_SIZE] gcc_aligned(KSTACK_SIZE);

extern uint8_t _binary___obj_user_guest_guest_start[];
extern uint8_t _binary___obj_user_idle_idle_start[];

static void kern_main_ap(void);

/*
 * The main function of the kernel on BSP and is called by kern_init().
 */
static void
kern_main(void)
{
	pageinfo_t *pi;
	struct pcpu *c;
	struct proc *guest_proc, *idle_proc;
	struct kstack *ap_kstack;
	int i;

	c = pcpu_cur();
	KERN_ASSERT(c != NULL && c->booted == TRUE);

	/* allocate memory for system call buffer */
	KERN_INFO("[BSP KERN] Prepare buffer for handling system calls ... ");
	if ((pi = mem_page_alloc()) == NULL)
		KERN_PANIC("Cannot allocate memory for buffer.\n");
	spinlock_acquire(&c->lk);
	c->sys_buf = (uint8_t *) mem_pi2phys(pi);
	spinlock_release(&c->lk);
	KERN_INFO("done.\n");

	/* register trap handlers */
	trap_init_array(c);
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
	trap_handler_register(T_IRQ0+IRQ_IPI_RESCHED, ipi_resched_handler);
	KERN_INFO("done.\n");

	/* enable interrupts */
	KERN_INFO("[BSP KERN] Enable TIMER interrupt ... ");
	intr_enable(IRQ_TIMER, 0);
	KERN_INFO("done.\n");

	KERN_INFO("[BSP KERN] Enable KBD interrupt ... ");
	kbd_intenable();
	KERN_INFO("done.\n");

	KERN_INFO("[BSP KERN] Enable IPI ... ");
	intr_enable(IRQ_IPI_RESCHED, 0);
	KERN_INFO("done.\n");

	/* create the first user process */
	guest_proc = proc_spawn(c, (uintptr_t)
			       _binary___obj_user_guest_guest_start);
	if (guest_proc == NULL)
		KERN_PANIC("Cannot create the guest process on BSP.\n");
	idle_proc = proc_spawn(c, (uintptr_t)
			       _binary___obj_user_idle_idle_start);
	if (idle_proc == NULL)
		KERN_PANIC("Cannot create the idle process on BSP.\n");


	/* boot APs  */
	for (i = 1; i < pcpu_ncpu(); i++) {
		KERN_INFO("Boot CPU%d ... ", i);

		if ((ap_kstack = kstack_alloc()) == NULL) {
			KERN_DEBUG("Cannot allocate memory for "
				   "kernel stack.\n");
			KERN_INFO("failed.\n");
			continue;
		}
		ap_kstack->cpu_idx = i;

		pcpu_boot_ap(i, kern_main_ap, (uintptr_t) ap_kstack);

		KERN_INFO("done.\n");
	}

	/* jump to userspace */
	KERN_INFO("[BSP KERN] Go to userspace ... \n");
	sched_lock(c);
	proc_sched(FALSE);

	KERN_PANIC("[BSP KERN] CertiKOS should not be here!\n");
}

static void
kern_main_ap(void)
{
	pageinfo_t *pi;
	struct pcpu *c;
	int cpu_idx;
	struct proc *init_proc;

	c = pcpu_cur();
	KERN_ASSERT(c != NULL && c->booted == FALSE);
	cpu_idx = pcpu_cpu_idx(c);

	/* allocate buffer for handling system calls */
	KERN_INFO("[AP%d KERN] Prepare buffer for handling system calls ... ",
		  cpu_idx);
	if ((pi = mem_page_alloc()) == NULL)
		KERN_PANIC("Cannot allocate memory for handling system calls.\n");
	spinlock_acquire(&c->lk);
	c->sys_buf = (uint8_t *) mem_pi2phys(pi);
	spinlock_release(&c->lk);
	KERN_INFO("done.\n");

	/* register trap handlers */
	trap_init_array(c);
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
	trap_handler_register(T_IRQ0+IRQ_IPI_RESCHED, ipi_resched_handler);
	KERN_INFO("done.\n");

	/* enable interrupts */
	KERN_INFO("[AP%d KERN] Enable TIMER interrupt ... ", cpu_idx);
	intr_enable(IRQ_TIMER, 0);
	KERN_INFO("done.\n");

	KERN_INFO("[AP%d KERN] Enable KBD interrupt ... ", cpu_idx);
	kbd_intenable();
	KERN_INFO("done.\n");

	KERN_INFO("[AP%d KERN] Enable IPI ... ", cpu_idx);
	intr_enable(IRQ_IPI_RESCHED, 0);
	KERN_INFO("done.\n");

	c->booted = TRUE;

	/* jump to userspace */
	KERN_INFO("[AP%d KERN] Go to userspace ... \n", cpu_idx);
	init_proc = proc_spawn(c,
			       (uintptr_t) _binary___obj_user_idle_idle_start);
	if (init_proc == NULL)
		KERN_PANIC("Cannot create idle process on AP%d.\n", cpu_idx);
	sched_lock(c);
	proc_sched(FALSE);

	KERN_PANIC("[AP%d KERN] CertiKOS should not be here.\n", cpu_idx);
}

/*
 * The C entry of the kernel on BSP and is called by start().
 */
void
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
	 *       |   +--------------+ <-- bsp_kstack + KSTACK_SIZE
	 *      BSS  |    kstack    |
	 *       |   +--------------+ <-- bsp_kstack
	 *       |   |              |
	 *       |   :      SBZ     :
	 *       |   |              |
	 *       \   +--------------+ <-- edata
	 *           |              |
	 *           :              :
	 */
	extern uint8_t end[], edata[];
	struct kstack *kstack = (struct kstack *) bsp_kstack;
	memzero(edata, bsp_kstack - edata);
	memzero(bsp_kstack + KSTACK_SIZE, end - bsp_kstack - KSTACK_SIZE);

	/*
	 * Initialize the console so that we can output debug messages to the
	 * screen and/or the serial port.
	 */
	cons_init();
	debug_init();
	KERN_INFO("Console is ready.\n");

	/*
	 * Initialize the bootstrap kernel stack, i.e. loading the bootstrap
	 * GDT, TSS and IDT, etc.
	 */
	KERN_INFO("Initialize bootstrap kernel stack ... ");
	kstack_init(kstack);
	kstack->cpu_idx = 0;
	KERN_INFO("done.\n");

	/*
	 * Initialize kernel memory allocator.
	 */
	KERN_INFO("Initialize kernel memory allocator ... ");
	mem_init(mbi);
	KERN_INFO("done.\n");

	/*
	 * Initialize PCPU module.
	 */
	KERN_INFO("Initialize PCPU module ... ");
	pcpu_init();
	KERN_INFO("done.\n");
	pcpu_cur()->kstack = kstack;
	pcpu_cur()->booted = TRUE;
	pcpu_init_cpu(); /* CPU specific initielization */

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
	 * Initialize virtual machine monitor module.
	 */
	if (strncmp(pcpu_cur()->arch_info.vendor, "AuthenticAMD", 20) == 0 ||
	    strncmp(pcpu_cur()->arch_info.vendor, "GenuineIntel", 20) == 0) {
		KERN_INFO("Initialize VMM ... ");
		if (vmm_init() != 0)
			KERN_INFO("failed.\n");
		else
			KERN_INFO("done.\n");
	}

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

	/* Start master kernel on BSP */
	KERN_INFO("Start kernel on BSP ... \n");
	kern_main();

	/* should not be here */
	KERN_PANIC("We should not be here.\n");
}

void
kern_init_ap(void (*f)(void))
{
	KERN_INFO("\n");

	struct kstack *ks =
		(struct kstack *) ROUNDDOWN(get_stack_pointer(), KSTACK_SIZE);

	kstack_init(ks);
	pcpu_cur()->kstack = ks;
	pcpu_init_cpu(); /* CPU specific initielization */
	pmap_init();
	intr_init();

	f();	/* kern_main_ap() */
}
