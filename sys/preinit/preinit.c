#include <lib/trap.h>
#include <lib/x86.h>

#include <preinit/dev/console.h>
#include <preinit/dev/ide.h>
#include <preinit/dev/intr.h>
#include <preinit/dev/tsc.h>
#include <preinit/dev/svm_drv.h>
#include <preinit/dev/vmx_drv.h>

#include <preinit/lib/mboot.h>
#include <preinit/lib/seg.h>
#include <preinit/lib/x86.h>
#include <preinit/lib/types.h>

#include <preinit/lib/debug.h>

cpu_vendor cpuvendor;

void
preinit(uintptr_t mbi_addr)
{
	seg_init();
    KERN_DEBUG("seg initialized.");

	enable_sse();
    KERN_DEBUG("sse enabled.");

	cons_init();
    KERN_DEBUG("cons initialized.");

	tsc_init();
    KERN_DEBUG("tsc initialized.");

	intr_init();
    KERN_DEBUG("intr initialized.");

	ide_init();
    KERN_DEBUG("ide initialized.");

    cpuvendor = vendor();

    if (cpuvendor == AMD) {
	    svm_hw_init();
        KERN_DEBUG("svm hw initialized.");
    }
    else if (cpuvendor == INTEL) {
        vmx_hw_init();
        KERN_DEBUG("vmx hw initialized.");
    }
    else {
        KERN_PANIC("unknown cpu vendor.");
    }

	/* enable interrupts */
	intr_enable(IRQ_TIMER);
	intr_enable(IRQ_KBD);
	intr_enable(IRQ_SERIAL13);

	pmmap_init(mbi_addr);
}
