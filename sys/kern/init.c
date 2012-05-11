#include <sys/console.h>
#include <sys/context.h>
#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/master.h>
#include <sys/mboot.h>
#include <sys/mem.h>
#include <sys/mmu.h>
#include <sys/pcpu.h>
#include <sys/slave.h>
#include <sys/string.h>
#include <sys/timer.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>

#include <machine/pmap.h>

#include <dev/pci.h>
#include <dev/tsc.h>
#include <dev/timer.h>

uint8_t pcpu_stack[MAX_CPU * PAGE_SIZE] gcc_aligned(PAGE_SIZE);

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
	context_init();
	pcpu_init_cpu();
	KERN_INFO("done.\n");

	/*
	 * Initialize virtual machine monitor module.
	 */
	KERN_INFO("Initialize VMM ... ");
	if (vmm_init() != 0)
		KERN_INFO("failed.\n");
	else
		KERN_INFO("done.\n");

	/* Start slave kernel on APs */
	int i;
	for (i = 1; i < pcpu_ncpu(); i++) {
		KERN_INFO("Start slave kernel on CPU%d ... ", i);
		pcpu_boot_ap(i, slave_kernel, (uintptr_t) &stack[i * PAGE_SIZE]);
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
	KERN_INFO("Start master kernel on BSP ... \n");
	master_kernel();

	/* should not be here */
	KERN_PANIC("We should not be here.\n");
}

void
kern_init_ap(void (*f)(void))
{
	pcpu_t *c = pcpu_cur();

	pcpu_init_cpu();
	intr_init();

	pmap_init();
	c->booted = TRUE;
	f();
}
