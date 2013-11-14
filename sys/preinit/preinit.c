#include <dev/ide.h>

#include <lib/trap.h>
#include <lib/types.h>
#include <lib/x86.h>

#include <preinit/dev/console.h>
#include <preinit/dev/intr.h>
#include <preinit/dev/tsc.h>
#include <preinit/dev/svm_drv.h>

#include <preinit/lib/mboot.h>
#include <preinit/lib/seg.h>
#include <preinit/lib/x86.h>

void
preinit(uintptr_t mbi_addr)
{
	seg_init();
	enable_sse();
	cons_init();
	tsc_init();
	intr_init();
	ide_init();
	svm_hw_init();

	/* enable interrupts */
	intr_enable(IRQ_TIMER);
	intr_enable(IRQ_KBD);
	intr_enable(IRQ_SERIAL13);

	pmmap_init(mbi_addr);
}
