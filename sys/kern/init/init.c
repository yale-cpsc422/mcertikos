#include <lib/debug.h>
#include <lib/export.h>
#include <dev/export.h>
#include <mm/export.h>
#include <proc/export.h>
#include <virt/export.h>

extern uint8_t _binary___obj_user_vmm_vmm_start[];
extern uint8_t _binary___obj_user_idle_idle_start[];

static void
kern_main(void)
{
	struct proc *idle_proc, *guest_proc;

	/* enable interrupts */
	intr_enable(IRQ_TIMER);
	intr_enable(IRQ_KBD);
	intr_enable(IRQ_SERIAL13);

	idle_proc = proc_create((uintptr_t) _binary___obj_user_idle_idle_start);
	if (idle_proc == NULL)
		KERN_PANIC("Cannot create the idle process.\n");

	KERN_DEBUG("idle process %d is created.\n", idle_proc->pid);

	guest_proc = proc_create((uintptr_t) _binary___obj_user_vmm_vmm_start);
	if (guest_proc == NULL)
		KERN_PANIC("Cannot create the VMM process.\n");

	KERN_DEBUG("vmm process %d is created.\n", guest_proc->pid);

	KERN_INFO("Start user-space ... \n");

	thread_sched();

	KERN_PANIC("kern_main() should never be here.\n");
}

void
kern_init(uintptr_t mbi_addr)
{
	/*
	 * Setup segments.
	 */
	seg_init();

	/*
	 * CompCert may use XMM registers though there's no SSE or floating
	 * instruction in the kernel, so we have to enable SSE in order to
	 * avoid the potential general protection fault when accessing XMM
	 * registers.
	 */
	enable_sse();

	/*
	 * Enable the console as soon as possible, so that we can output
	 * debug messages to the screen and/or the serial port.
	 */
	cons_init();
	KERN_INFO("Console is ready.\n");


	/*
	 * Detect CPU.
	 */
	KERN_INFO("Detect CPU and APIC ... ");
	pcpu_init();
	KERN_INFO("done.\n");

	/*
	 * Initialize the virtual memory.
	 */
	KERN_INFO("Initialize memory management module ... ");
	pmap_init(mbi_addr);
	KERN_INFO("done.\n");

	/*
	 * Initialize the interrupt controllers.
	 */
	KERN_INFO("Initialize interrupt controller ... ");
	intr_init();
	KERN_INFO("done.\n");

	/*
	 * Initialize the process management.
	 */
	KERN_INFO("Initialize process management module ... ");
	proc_init();
	KERN_INFO("done.\n");

	/*
	 * Initialize the hardware virtualization driver.
	 */
	KERN_INFO("Initialize virtualization module ... ");
	hvm_init();
	KERN_INFO("done.\n");

	/*
	 * Initialize the IDE disk driver.
	 */
	KERN_INFO("Detect IDE disk driver ... ");
	ide_init();
	KERN_INFO("done.\n");

	kern_main();
}
