#include <lib/gcc.h>
#include <lib/string.h>
#include <preinit/lib/types.h>

#include <preinit/lib/debug.h>
#include <preinit/lib/x86.h>

#include <preinit/dev/svm_drv.h>

#include <kern/virt/npt_intro.h>


#define PAGESIZE		4096
#define SVM_IOPM_SIZE		(1 << 12)
#define MSR_VM_HSAVE_PA		0xc0010117
#define VMCB_SIZE		4096
#define XVMST_SIZE		6

struct host_ctx {
	unsigned int ebx;
	unsigned int ecx;
	unsigned int edx;
	unsigned int esi;
	unsigned int edi;
	unsigned int ebp;
};

static uint8_t hsave_area[PAGESIZE] gcc_aligned(PAGESIZE);
static uint8_t iopm0[SVM_IOPM_SIZE] gcc_aligned(PAGESIZE);

extern struct NPTStruct NPT_LOC gcc_aligned(PAGESIZE);
extern unsigned int VMCB_LOC[VMCB_SIZE] gcc_aligned(VMCB_SIZE);
extern unsigned int XVMST_LOC[XVMST_SIZE];
extern struct host_ctx h_ctx0;

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
	memzero(VMCB_LOC, PAGESIZE);
	memset(iopm0, 0xff, SVM_IOPM_SIZE);
	VMCB_LOC[16] = (uintptr_t) iopm0;
	VMCB_LOC[44] = (uintptr_t) &NPT_LOC;

	/* initialize extra VM state and the host context */
	memzero(XVMST_LOC, sizeof(unsigned int) * XVMST_SIZE);
	memzero(&h_ctx0, sizeof(struct host_ctx));

	return 0;
}

extern void enter_guest(void *vmcb,
			uint32_t *g_ebx, uint32_t *g_ecx, uint32_t *g_edx,
			uint32_t *g_esi, uint32_t *g_edi, uint32_t *g_ebp,
			uint32_t *h_ebx, uint32_t *h_ecx, uint32_t *h_edx,
			uint32_t *h_esi, uint32_t *h_edi, uint32_t *h_ebp);

void
host_out(void)
{
	enter_guest(VMCB_LOC,
		    &XVMST_LOC[0], &XVMST_LOC[1], &XVMST_LOC[2],
		    &XVMST_LOC[3], &XVMST_LOC[4], &XVMST_LOC[5],
		    &h_ctx0.ebx, &h_ctx0.ecx, &h_ctx0.edx,
		    &h_ctx0.esi, &h_ctx0.edi, &h_ctx0.ebp);
}

void
host_int(void)
{
}
