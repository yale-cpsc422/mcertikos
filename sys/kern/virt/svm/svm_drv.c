#include <lib/debug.h>
#include <lib/gcc.h>
#include <lib/string.h>
#include <lib/types.h>
#include <virt/hvm.h>
#include <lib/x86.h>

#include <dev/intr.h>
#include <dev/pcpu.h>

#include "svm.h"

static uint8_t hsave_area[PAGESIZE] gcc_aligned(PAGESIZE);

/*
 * Check whether the machine supports SVM. (Sec 15.4, APM Vol2 r3.19)
 *
 * @return TRUE if SVM is supported; otherwise FALSE
 */
static bool
svm_check(void)
{
	/* check CPUID 0x80000001 */
	uint32_t feature, dummy;
	cpuid(CPUID_FEATURE_FUNC, &dummy, &dummy, &feature, &dummy);
	if ((feature & CPUID_X_FEATURE_SVM) == 0) {
		SVM_DEBUG("The processor does not support SVM.\n");
		return FALSE;
	}

	/* check MSR VM_CR */
	if ((rdmsr(MSR_VM_CR) & MSR_VM_CR_SVMDIS) == 0) {
		return TRUE;
	}

	/* check CPUID 0x8000000a */
	cpuid(CPUID_SVM_FEATURE_FUNC, &dummy, &dummy, &dummy, &feature);
	if ((feature & CPUID_SVM_LOCKED) == 0) {
		SVM_DEBUG("SVM maybe disabled by BIOS.\n");
		return FALSE;
	} else {
		SVM_DEBUG("SVM maybe disabled with key.\n");
		return FALSE;
	}

	SVM_DEBUG("SVM is available.\n");

	return TRUE;
}

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

	SVM_DEBUG("SVM is enabled.\n");
}

int
svm_drv_init(void)
{
	/* check whether the processor supports SVM */
	if (svm_check() == FALSE)
		return 1;
	/* enable SVM */
	svm_enable();
	SVM_DEBUG("Host state-save area is at %x.\n", hsave_addr);
	memzero(hsave_area, PAGESIZE);
	wrmsr(MSR_VM_HSAVE_PA, (uintptr_t) hsave_area);
	return 0;
}

/* defined in svm_asm.S */
extern void svm_run(struct vmcb *vmcb, uint32_t *ebx, uint32_t *ecx,
		    uint32_t *edx, uint32_t *esi, uint32_t *edi, uint32_t *ebp);

#define save_host_segment(seg, val)					\
	__asm __volatile("mov %%" #seg ", %0" : "=r" (val) :: "memory")

#define load_host_segment(seg, val)				\
	do {							\
		uint16_t _val = (uint16_t) val;			\
		__asm __volatile("movl %k0, %%" #seg		\
				 : "+r" (_val) :: "memory");	\
	} while (0)

void
svm_drv_run_vm(struct vmcb *vmcb, uint32_t *ebx, uint32_t *ecx,
	       uint32_t *edx, uint32_t *esi, uint32_t *edi, uint32_t *ebp)
{
	volatile uint16_t h_fs, h_gs, h_ldt;

	SVM_CLGI();

	save_host_segment(fs, h_fs);
	save_host_segment(gs, h_gs);
	h_ldt = rldt();

	intr_local_enable();
	svm_run(vmcb, ebx, ecx, edx, esi, edi, ebp);
	intr_local_disable();

	load_host_segment(fs, h_fs);
	load_host_segment(gs, h_gs);
	lldt(h_ldt);

	SVM_STGI();

	return;
}

#ifdef DEBUG_MSG

int
svm_handle_err(struct vmcb *vmcb)
{
	KERN_ASSERT(vmcb != NULL);

	struct vmcb_save_area *save = &vmcb->save;
	struct vmcb_control_area *ctrl = &vmcb->control;

	/* Sec 3.1.7, APM Vol2 r3.19 */
	uint64_t efer = save->efer;
	if ((efer & MSR_EFER_SVME) == 0)
		KERN_WARN("EFER.SVME != 0\n");
	if ((efer >> 32) != 0ULL)
		KERN_WARN("EFER[63..32] != 0\n");
	if ((efer >> 15) != 0ULL)
		KERN_WARN("EFER[31..15] != 0\n");
	if ((efer & 0xfe) != 0ULL)
		KERN_WARN("EFER[7..1] != 0\n");

	uint64_t cr0 = save->cr0;
	if ((cr0 & CR0_CD) == 0 && (cr0 & CR0_NW))
		KERN_WARN("CR0.CD = 0 && CR0.NW = 1\n");
	if ((cr0 >> 32) != 0ULL)
		KERN_WARN("CR0[63..32] != 0\n");

	/*
	 * TODO: check the MBZ bits of CR3 according to the actual mode
	 *       CertiKOS and/or VM is running in. Current version CertiKOS
	 *       and VM are using the legacy-mode non-PAE paging, so there
	 *       is no MBZ bits in CR3. (Sec 3.1.2, APM Vol2 r3.19)
	 */

	/* Sec 3.1.3, APM Vol2 r3.19 */
	uint64_t cr4 = save->cr4;
	if ((cr4 >> 32) != 0ULL)
		KERN_WARN("CR4[63..32] != 0\n");
	if ((cr4 >> 19) != 0ULL)
		KERN_WARN("CR4[31..19] != 0\n");
	if (((cr4 >> 11) & 0x7fULL) != 0ULL)
		KERN_WARN("CR4[17..11] != 0\n");

	if ((save->dr6 >> 32) != 0ULL)
		KERN_WARN("DR6[63..32] != 0\n");

	if ((save->dr7 >> 32) != 0ULL)
		KERN_WARN("DR7[63..32] != 0\n");

	if (((efer & MSR_EFER_LMA) || (efer & MSR_EFER_LME)) &&
	    (pcpu_cur()->arch_info.feature2 & CPUID_X_FEATURE_LM) == 0)
		KERN_WARN("EFER.LMA =1 or EFER.LME = 1, "
			  "while long mode is not supported.\n");
	if ((efer & MSR_EFER_LME) && (cr0 & CR0_PG) && (cr4 & CR4_PAE) == 0)
		KERN_WARN("EFER.LME = 1 and CR0.PG =1, while CR4.PAE = 0.\n");

	if ((efer & MSR_EFER_LME) && (cr0 & CR0_PG) && (cr0 & CR0_PE) == 0)
		KERN_WARN("EFER.LME = 1 and CR0.PG =1, while CR0.PE = 0.\n");

	uint16_t cs_attrib = save->cs.attrib;

	if ((efer & MSR_EFER_LME) && (cr0 & CR0_PG) && (cr4 & CR4_PAE) &&
	    (cs_attrib & SEG_ATTR_L) && (cs_attrib & SEG_ATTR_D))
		KERN_WARN("EFER.LME, CR0.PG, CR4.PAE, CS.L and CS.D "
			  "are non-zero.\n");

	if ((ctrl->intercept & (1ULL << INTERCEPT_VMRUN)) == 0)
		KERN_WARN("VMRUN is not intercepted.\n");

	if (ctrl->iopm_base_pa > mem_max_phys())
		KERN_WARN("The address of IOPM (%x) is out of range of "
			  "the physical memory.\n",
			  ctrl->iopm_base_pa);

	if (ctrl->msrpm_base_pa > mem_max_phys())
		KERN_WARN("The address of MSRPM (%x) is out of range of "
			  "the physical memory.\n",
			  ctrl->msrpm_base_pa);

	if (ctrl->event_inj & SVM_EVTINJ_VALID)
		KERN_WARN("Illegal event injection.\n");

	if (ctrl->asid == 0)
		KERN_WARN("ASID = 0\n");

	return 0;
}

#endif /* DEBUG_MSG */
