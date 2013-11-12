#include <lib/gcc.h>
#include <lib/string.h>
#include <lib/types.h>
#include <lib/x86.h>

#include <preinit/lib/debug.h>

#include <preinit/dev/svm_drv.h>

#include <kern/virt/vmcb.h>

#define PAGESIZE		4096

#define MSR_VM_HSAVE_PA		0xc0010117

static uint8_t hsave_area[PAGESIZE] gcc_aligned(PAGESIZE);

extern struct vmcb vmcb0 gcc_aligned(4096);
extern uint32_t npt_lv1[1024] gcc_aligned(4096);
static uint8_t iopm0[SVM_IOPM_SIZE] gcc_aligned(4096);

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
svm_hw_init(void)
{
	/* enable SVM */
	svm_enable();
	KERN_DEBUG("Host state-save area is at %x.\n", hsave_area);
	memzero(hsave_area, PAGESIZE);
	wrmsr(MSR_VM_HSAVE_PA, (uintptr_t) hsave_area);

	/* initialize VMCB */
	memzero(&vmcb0, sizeof(struct vmcb));
	memset(iopm0, 0xff, SVM_IOPM_SIZE);
	vmcb0.control.iopm_base_pa_lo = (uintptr_t) iopm0;
	vmcb0.control.iopm_base_pa_hi = 0;
	/* setup NPT permanently */
	vmcb0.control.nested_cr3_lo = (uintptr_t) npt_lv1;
	vmcb0.control.nested_cr3_hi = 0;


	return 0;
}
