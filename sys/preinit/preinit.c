#include <lib/trap.h>
#include <lib/x86.h>

#include <preinit/dev/console.h>
#include <preinit/dev/disk.h>
#include <preinit/dev/intr.h>
#include <preinit/dev/pci.h>
#include <preinit/dev/tsc.h>
#include <preinit/dev/svm_drv.h>
#include <preinit/dev/vmx_drv.h>

#include <preinit/lib/mboot.h>
#include <preinit/lib/seg.h>
#include <preinit/lib/x86.h>
#include <preinit/lib/types.h>

#include <preinit/lib/debug.h>
#include <preinit/lib/timing.h>

#include <kern/trap/sysenter.h>

cpu_vendor cpuvendor;

void
set_vendor()
{
    cpuvendor = vendor();
}

void
preinit (uintptr_t mbi_addr)
{
    seg_init ();

    enable_sse ();

    cons_init ();
    KERN_DEBUG("cons initialized.\n");

    tsc_init ();
    KERN_DEBUG("tsc initialized.\n");

#ifdef PROFILING
    profiling_init ();
    KERN_DEBUG("profiling initialized.\n");
#endif

    sysenter_init();
    KERN_DEBUG("sysenter initialized.\n");

    intr_init ();
    KERN_DEBUG("intr initialized.\n");

    /* 	ide_init(); */
    /* KERN_DEBUG("ide initialized.\n"); */

    disk_init ();
    pci_init ();

    set_vendor ();
    if (cpuvendor == AMD)
    {
        KERN_DEBUG("vendor detected: AMD.\n");
        svm_hw_init ();
        KERN_DEBUG("svm hw initialized.\n");
    }
    else if (cpuvendor == INTEL)
    {
        KERN_DEBUG("vendor detected: INTEL.\n");
    }
    else
    {
        KERN_PANIC("unknown cpu vendor.\n");
    }

    /* enable interrupts */
    intr_enable (IRQ_TIMER);
    intr_enable (IRQ_KBD);
    intr_enable (IRQ_SERIAL13);

    pmmap_init (mbi_addr);
}
