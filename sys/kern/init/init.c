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

	guest_proc = proc_create((uintptr_t) _binary___obj_user_vmm_vmm_start);
	if (guest_proc == NULL)
		KERN_PANIC("Cannot create the VMM process.\n");

	thread_sched();

	KERN_PANIC("kern_main() should never be here.\n");
}

void
kern_init(struct mboot_info *mbi)
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

	/*
	 * Detect CPU.
	 */
	pcpu_init();

	/*
	 * Initialize the physical memory allocator.
	 */
	mem_init(mbi);

	/*
	 * Initialize the virtual memory.
	 */
	pmap_init();

	/*
	 * Initialize the interrupt controllers.
	 */
	intr_init();

	/*
	 * Initialize the process management.
	 */
	proc_init();

	/*
	 * Initialize the hardware virtualization driver.
	 */
	hvm_init();

	/*
	 * Initialize the IDE disk driver.
	 */
	ide_init();
}
