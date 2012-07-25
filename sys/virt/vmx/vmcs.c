#include <sys/debug.h>
#include <sys/mem.h>
#include <sys/mmu.h>
#include <sys/pcpu.h>
#include <sys/trap.h>
#include <sys/types.h>
#include <sys/x86.h>

#include "ept.h"
#include "vmcs.h"
#include "x86.h"

typedef
enum {CS, SS, DS, ES, FS, GS, LDTR, TR, GDTR, IDTR} seg_t;

static struct {
	uint32_t encoding_selector;
	uint32_t encoding_base;
	uint32_t encoding_limit;
	uint32_t encoding_access;
} segs[IDTR+1] = {
	[CS] = { VMCS_GUEST_CS_SELECTOR, VMCS_GUEST_CS_BASE,
		 VMCS_GUEST_CS_LIMIT, VMCS_GUEST_CS_ACCESS_RIGHTS },
	[SS] = { VMCS_GUEST_SS_SELECTOR, VMCS_GUEST_SS_BASE,
		 VMCS_GUEST_SS_LIMIT, VMCS_GUEST_SS_ACCESS_RIGHTS },
	[DS] = { VMCS_GUEST_DS_SELECTOR, VMCS_GUEST_DS_BASE,
		 VMCS_GUEST_DS_LIMIT, VMCS_GUEST_DS_ACCESS_RIGHTS },
	[ES] = { VMCS_GUEST_ES_SELECTOR, VMCS_GUEST_ES_BASE,
		 VMCS_GUEST_ES_LIMIT, VMCS_GUEST_ES_ACCESS_RIGHTS },
	[FS] = { VMCS_GUEST_FS_SELECTOR, VMCS_GUEST_FS_BASE,
		 VMCS_GUEST_FS_LIMIT, VMCS_GUEST_FS_ACCESS_RIGHTS },
	[GS] = { VMCS_GUEST_GS_SELECTOR, VMCS_GUEST_GS_BASE,
		 VMCS_GUEST_GS_LIMIT, VMCS_GUEST_GS_ACCESS_RIGHTS },
	[LDTR] = { VMCS_GUEST_LDTR_SELECTOR, VMCS_GUEST_LDTR_BASE,
		   VMCS_GUEST_LDTR_LIMIT, VMCS_GUEST_LDTR_ACCESS_RIGHTS },
	[TR] = { VMCS_GUEST_TR_SELECTOR, VMCS_GUEST_TR_BASE,
		 VMCS_GUEST_TR_LIMIT, VMCS_GUEST_TR_ACCESS_RIGHTS },
	[GDTR] = { 0, VMCS_GUEST_GDTR_BASE, VMCS_GUEST_GDTR_LIMIT, 0 },
	[IDTR] = { 0, VMCS_GUEST_IDTR_BASE, VMCS_GUEST_IDTR_LIMIT, 0 },
};

static int
vmcs_set_guest_segment(seg_t seg, uint16_t sel, uintptr_t base,
		       uint32_t lim, uint32_t access)
{
	int error = 0;

	KERN_ASSERT(CS <= seg && seg <= IDTR);

	if ((error = vmcs_write(segs[seg].encoding_base, base)))
		goto done;
	if ((error = vmcs_write(segs[seg].encoding_limit, lim)))
		goto done;

	if (seg != GDTR && seg != IDTR) {
		if ((error = vmcs_write(segs[seg].encoding_access, access)))
			goto done;

		if ((error = vmcs_write(segs[seg].encoding_selector, sel)))
			goto done;
	}

 done:
	return error;
}

/*
 * Read a field in VMCS structure.
 *
 * @param encoding the encoding of the VMCS field
 *
 * @return the value of the field; the higher bits will be zeroed, if
 *         the width of the field is less than 64 bits.
 */
uint64_t
vmcs_read(uint32_t encoding)
{
	KERN_ASSERT((encoding & (uint32_t) 1) == 0);

	uint32_t val_hi, val_lo;
	int error;

	val_lo = val_hi = 0;

	switch (encoding) {
		/* 64-bit fields */
	case VMCS_IO_BITMAP_A ... VMCS_EPTP:
	case VMCS_GUEST_PHYSICAL_ADDRESS:
	case VMCS_LINK_POINTER ... VMCS_GUEST_PDPTE3:
	case VMCS_HOST_IA32_PAT ... VMCS_HOST_IA32_PERF_GLOBAL_CTRL:
		error = vmread(encoding, &val_lo);
		if (error)
			break;
		encoding += 1;
		error = vmread(encoding, &val_hi);
		break;

		/* 16-bit, 32-bit and natural-width fields */
	default:
		error = vmread(encoding, &val_lo);
		break;
	}

	if (error)
		KERN_PANIC("vmcs_read(encoding 0x%08x) error %d.\n",
			   encoding, error);

	return (((uint64_t) val_hi << 32) | (uint64_t) val_lo);
}

/*
 * Write to a fileld in VMCS structure.
 *
 * @param encoding the encoding of the VMCS field
 * @param val the value to be written; the higher bits will be masked as 0's. if
 *            width of the field is less than 64 bits.
 */
int
vmcs_write(uint32_t encoding, uint64_t val)
{
	KERN_ASSERT((encoding & (uint32_t) 1) == 0);

	int error = 0;

	switch (encoding) {
		/* 64-bit fields */
	case VMCS_IO_BITMAP_A ... VMCS_EPTP:
	case VMCS_GUEST_PHYSICAL_ADDRESS:
	case VMCS_LINK_POINTER ... VMCS_GUEST_PDPTE3:
	case VMCS_HOST_IA32_PAT ... VMCS_HOST_IA32_PERF_GLOBAL_CTRL:
		error = vmwrite(encoding, (uint32_t) val);
		if (error)
			break;
		encoding += 1;
		error = vmwrite(encoding, (uint32_t) (val >> 32));
		break;

		/* 16-bit, 32-bit and natural-width fields */
	default:
		error = vmwrite(encoding, (uint32_t) val);
		break;
	}

	if (error)
		KERN_PANIC("vmcs_write(encoding 0x%08x, val 0x%llx) error %d.\n",
			   encoding, val, error);

	return error;
}

int
vmcs_set_defaults(struct vmcs *vmcs, uint64_t *pml4ept, uint32_t pinbased_ctls,
		  uint32_t procbased_ctls, uint32_t procbased_ctls2,
		  uint32_t exit_ctls, uint32_t entry_ctls,
		  char *msr_bitmap, char *io_bitmap_a, char *io_bitmap_b,
		  uint16_t vpid, uint64_t cr0_ones_mask, uint64_t cr0_zeros_mask,
		  uint64_t cr4_ones_mask, uint64_t cr4_zeros_mask,
		  uintptr_t host_rip)
{
	int error;
	pcpu_t *c;
	extern gatedesc_t idt[];

	KERN_ASSERT(vmcs != NULL && pml4ept != NULL && msr_bitmap != NULL);
	KERN_ASSERT(vpid > 0);

	c = pcpu_cur();
	KERN_ASSERT(c != NULL);

	/*
	 * Make sure we have a "current" VMCS to work with.
	 */
	vmptrld(vmcs);

	/*
	 * Load the VMX controls
	 */
	KERN_DEBUG("Set pin-based controls to 0x%08x\n", pinbased_ctls);
	if ((error = vmcs_write(VMCS_PIN_BASED_CTLS, pinbased_ctls)) != 0)
		goto done;
	KERN_DEBUG("Set primary processor-based controls to 0x%08x\n",
		   procbased_ctls);
	if ((error = vmcs_write(VMCS_PRI_PROC_BASED_CTLS, procbased_ctls)) != 0)
		goto done;
	KERN_DEBUG("Set secondary processor-based controls to 0x%08x\n",
		   procbased_ctls2);
	if ((error = vmcs_write(VMCS_SEC_PROC_BASED_CTLS, procbased_ctls2)) != 0)
		goto done;
	KERN_DEBUG("Set exit controls to 0x%08x\n", exit_ctls);
	if ((error = vmcs_write(VMCS_EXIT_CTLS, exit_ctls)) != 0)
		goto done;
	KERN_DEBUG("Set entry controls to 0x%08x\n", entry_ctls);
	if ((error = vmcs_write(VMCS_ENTRY_CTLS, entry_ctls)) != 0)
		goto done;

	/*
	 * Load host state. (Intel Software Developer's Manual Vol 3, Sec 24.5)
	 */
	/* host rip */
	if ((error = vmcs_write(VMCS_HOST_RIP, host_rip)) != 0)
		goto done;

	/* host control registers */
	if ((error = vmcs_write(VMCS_HOST_CR0, rcr0())) != 0)
		goto done;
	if ((error = vmcs_write(VMCS_HOST_CR3, rcr3())) != 0)
		goto done;
	if ((error = vmcs_write(VMCS_HOST_CR4, rcr4())) != 0)
		goto done;

	/* host segment selectors */
	if ((error = vmcs_write(VMCS_HOST_ES_SELECTOR, CPU_GDT_KDATA)) != 0)
		goto done;
	if ((error = vmcs_write(VMCS_HOST_CS_SELECTOR, CPU_GDT_KCODE)) != 0)
		goto done;
	if ((error = vmcs_write(VMCS_HOST_SS_SELECTOR, CPU_GDT_KDATA)) != 0)
		goto done;
	if ((error = vmcs_write(VMCS_HOST_DS_SELECTOR, CPU_GDT_KDATA)) != 0)
		goto done;
	if ((error = vmcs_write(VMCS_HOST_FS_SELECTOR, CPU_GDT_KDATA)) != 0)
		goto done;
	if ((error = vmcs_write(VMCS_HOST_GS_SELECTOR, CPU_GDT_KDATA)) != 0)
		goto done;
	if ((error = vmcs_write(VMCS_HOST_TR_SELECTOR, CPU_GDT_TSS)) != 0)
		goto done;

	/* host segment base address */
	if ((error = vmcs_write(VMCS_HOST_FS_BASE, 0)))
		goto done;
	if ((error = vmcs_write(VMCS_HOST_GS_BASE, 0)))
		goto done;
	if ((error = vmcs_write(VMCS_HOST_TR_BASE, (uintptr_t) &c->_pcpu->tss)))
		goto done;
	if ((error = vmcs_write(VMCS_HOST_GDTR_BASE, (uintptr_t) &c->_pcpu->gdt)))
		goto done;
	if ((error = vmcs_write(VMCS_HOST_IDTR_BASE, (uintptr_t) idt)))
		goto done;

	/* host control registers */
	if ((error = vmcs_write(VMCS_HOST_IA32_SYSENTER_CS,
			     (uint32_t) rdmsr(MSR_IA32_SYSENTER_CS))))
		goto done;
	if ((error = vmcs_write(VMCS_HOST_IA32_SYSENTER_ESP,
			     (uintptr_t) rdmsr(MSR_IA32_SYSENTER_ESP))))
		goto done;
	if ((error = vmcs_write(VMCS_HOST_IA32_SYSENTER_EIP,
			     (uintptr_t) rdmsr(MSR_IA32_SYSENTER_EIP))))
		goto done;
	if ((error = vmcs_write(VMCS_HOST_IA32_PERF_GLOBAL_CTRL,
			     rdmsr(MSR_IA32_PERF_GLOBAL_CTRL))))
		goto done;
	if ((error = vmcs_write(VMCS_HOST_IA32_PAT, rdmsr(MSR_PAT))))
		goto done;
	if ((error = vmcs_write(VMCS_HOST_IA32_EFER, rdmsr(MSR_EFER))))
		goto done;

	/*
	 * Load guest state. (Intel Software Developer's Manual Vol3, Sec 24.4,
	 * Sec 9.1)
	 */
	/* guest control registers */
	if ((error = vmcs_write(VMCS_GUEST_CR0, 0x60000010 | CR0_NE)))
		goto done;
	if ((error = vmcs_write(VMCS_GUEST_CR3, 0)))
		goto done;
	if ((error = vmcs_write(VMCS_GUEST_CR4, CR4_VMXE)))
		goto done;

	/* guest debug registers */
	if ((error = vmcs_write(VMCS_GUEST_DR7, 0x00000400)))
		goto done;

	/* guest RSP, RIP, RFLAGS */
	if ((error = vmcs_write(VMCS_GUEST_RSP, 0)))
		goto done;
	if ((error = vmcs_write(VMCS_GUEST_RIP, 0x0000fff0)))
		goto done;
	if ((error = vmcs_write(VMCS_GUEST_RFLAGS, 0x00000002)))
		goto done;

	/* guest segment */
#define CODE_SEG_AR (SEG_ATTR_P | SEG_ATTR_S | SEG_TYPE_CODE | SEG_ATTR_A)
#define DATA_SEG_AR (SEG_ATTR_P | SEG_ATTR_S | SEG_TYPE_DATA | SEG_ATTR_A)
#define LDT_AR      (SEG_ATTR_P | SEG_TYPE_LDT)
#define TSS_AR      (SEG_ATTR_P | SEG_TYPE_TSS)
	if ((error = vmcs_set_guest_segment(CS, 0xf000, 0x000f0000, 0xffff,
					    CODE_SEG_AR)))
		goto done;
	if ((error = vmcs_set_guest_segment(SS, 0x0000, 0, 0xffff,
					    DATA_SEG_AR)))
		goto done;
	if ((error = vmcs_set_guest_segment(DS, 0, 0, 0xffff, DATA_SEG_AR)))
		goto done;
	if ((error = vmcs_set_guest_segment(ES, 0, 0, 0xffff, DATA_SEG_AR)))
		goto done;
	if ((error = vmcs_set_guest_segment(FS, 0, 0, 0xffff, DATA_SEG_AR)))
		goto done;
	if ((error = vmcs_set_guest_segment(GS, 0, 0, 0xffff, DATA_SEG_AR)))
		goto done;
	if ((error = vmcs_set_guest_segment(LDTR, 0, 0, 0xffff, LDT_AR)))
		goto done;
	if ((error = vmcs_set_guest_segment(TR, 0, 0, 0xffff, TSS_AR)))
		goto done;
	if ((error = vmcs_set_guest_segment(GDTR, 0, 0, 0xffff, 0)))
		goto done;
	if ((error = vmcs_set_guest_segment(IDTR, 0x0000, 0, 0xffff, 0)))
		goto done;
#undef CODE_SEG_AR
#undef DATA_SEG_AR
#undef LDT_AR
#undef TSS_AR

	/* guest MSRs */
	if ((error = vmcs_write(VMCS_GUEST_IA32_PAT, 0x7040600070406ULL)))
		goto done;
	if ((error = vmcs_write(VMCS_GUEST_IA32_SYSENTER_CS, 0)))
		goto done;
	if ((error = vmcs_write(VMCS_GUEST_IA32_SYSENTER_ESP, 0)))
		goto done;
	if ((error = vmcs_write(VMCS_GUEST_IA32_SYSENTER_EIP, 0)))
		goto done;
	if ((error = vmcs_write(VMCS_GUEST_IA32_DEBUGCTL, 0)))
		goto done;

	/* EPTP */
	if ((error = vmcs_write(VMCS_EPTP, EPTP((uintptr_t) pml4ept))))
		goto done;

	/* VPID */
	if ((error = vmcs_write(VMCS_VPID, vpid)))
		goto done;

	/* exception bitmap */
	if ((error = vmcs_write(VMCS_EXCEPTION_BITMAP, (1 << T_MCHK))))
		goto done;

	/* MSR bitmap */
	if ((error = vmcs_write(VMCS_MSR_BITMAP, (uintptr_t) msr_bitmap)))
		goto done;

	/* link pointer */
	if ((error = vmcs_write(VMCS_LINK_POINTER, 0xffffffffffffffffULL)))
		goto done;

	/* CR0 mask & shadow */
	if ((error = vmcs_write(VMCS_CR0_MASK, cr0_ones_mask | cr0_zeros_mask)))
		goto done;
	if ((error = vmcs_write(VMCS_CR0_SHADOW, cr0_ones_mask)))
		goto done;

	/* CR4 mask & shadow */
	if ((error = vmcs_write(VMCS_CR4_MASK, cr4_ones_mask | cr4_zeros_mask)))
		goto done;
	if ((error = vmcs_write(VMCS_CR4_SHADOW, cr4_ones_mask)))
		goto done;

	/* I/O bitmap */
	if ((error = vmcs_write(VMCS_IO_BITMAP_A, (uintptr_t) io_bitmap_a)))
		goto done;
	if ((error = vmcs_write(VMCS_IO_BITMAP_B, (uintptr_t) io_bitmap_b)))
		goto done;

	/* others */
	if ((error = vmcs_write(VMCS_GUEST_ACTIVITY, 0)))
		goto done;
	if ((error = vmcs_write(VMCS_GUEST_INTERRUPTIBILITY, 0)))
		goto done;
	if ((error = vmcs_write(VMCS_GUEST_PENDING_DBG_EXCEPTIONS, 0)))
		goto done;
	if ((error = vmcs_write(VMCS_ENTRY_INTR_INFO, 0)))
		goto done;

 done:
	if (error)
		dprintf("failed.\n");

	vmclear(vmcs);
	return error;
}
