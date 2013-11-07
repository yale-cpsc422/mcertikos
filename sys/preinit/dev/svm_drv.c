#include <lib/string.h>
#include <lib/types.h>
#include <lib/x86.h>

#include <preinit/lib/debug.h>
#include <preinit/lib/gcc.h>

#include "svm_drv.h"

#define PAGESIZE		4096

#define MSR_VM_HSAVE_PA		0xc0010117

static uint8_t hsave_area[PAGESIZE] gcc_aligned(PAGESIZE);

/*
 * Enable SVM. (Sec 15.4, APM Vol2 r3.19)
 */
static void
svm_enable(void)
{
	/* set MSR_EFER.SVME */
	uint64_t efer;

	efer = rdmsr(MSR_EFER);
	efer |= MSR_EFER_SVME;
	wrmsr(MSR_EFER, efer);

	KERN_DEBUG("SVM is enabled.\n");
}

int
svm_drv_init(void)
{
	/* enable SVM */
	svm_enable();
	KERN_DEBUG("Host state-save area is at %x.\n", hsave_area);
	memzero(hsave_area, PAGESIZE);
	wrmsr(MSR_VM_HSAVE_PA, (uintptr_t) hsave_area);
	return 0;
}
