#include <preinit/lib/debug.h>
#include <preinit/dev/intr.h>

#include <lib/seg.h>
#include <lib/types.h>
#include <lib/x86.h>

#include <dev/ide.h>

#include <preinit/preinit.h>

#include <mm/export.h>
#include <proc/export.h>
#include <virt/export.h>

extern uint8_t _binary___obj_user_vmm_vmm_start[];
extern uint8_t _binary___obj_user_idle_idle_start[];

static void
kern_main(void)
{
	struct proc *idle_proc, *guest_proc;

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
	preinit();

	/*
	 * Initialize the virtual memory.
	 */
	KERN_INFO("Initialize memory management module ... ");
	pmap_init(mbi_addr);
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
	svm_init();
	KERN_INFO("done.\n");

	kern_main();
}
