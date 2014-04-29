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
    KERN_DEBUG("seg initialized.\n");

	enable_sse();
    KERN_DEBUG("sse enabled.\n");

	cons_init();
    KERN_DEBUG("cons initialized.\n");

	tsc_init();
    KERN_DEBUG("tsc initialized.\n");

	intr_init();
    KERN_DEBUG("intr initialized.\n");

	ide_init();
    KERN_DEBUG("ide initialized.\n");

    cpuvendor = vendor();

    if (cpuvendor == AMD) {
        KERN_DEBUG("vendor detected: AMD.\n");
	    svm_hw_init();
        KERN_DEBUG("svm hw initialized.\n");
    }
    else if (cpuvendor == INTEL) {
        KERN_DEBUG("vendor detected: INTEL.\n");
        vmx_hw_init();
        KERN_DEBUG("vmx hw initialized.\n");
    }
    else {
        KERN_PANIC("unknown cpu vendor.\n");
    }

	/* enable interrupts */
	intr_enable(IRQ_TIMER);
	intr_enable(IRQ_KBD);
	intr_enable(IRQ_SERIAL13);

	pmmap_init(mbi_addr);
}
