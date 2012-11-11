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

gcc_inline uint16_t
vmcs_read16(uint32_t encoding)
{
	return (uint16_t) vmread(encoding);
}

gcc_inline uint32_t
vmcs_read32(uint32_t encoding)
{
	return vmread(encoding);
}

gcc_inline uint64_t
vmcs_read64(uint32_t encoding)
{
	return vmread(encoding) |
		((uint64_t) vmread(encoding+1) << 32);
}

gcc_inline void
vmcs_write16(uint32_t encoding, uint16_t val)
{
	vmwrite(encoding, val);
}

gcc_inline void
vmcs_write32(uint32_t encoding, uint32_t val)
{
	vmwrite(encoding, val);
}

gcc_inline void
vmcs_write64(uint32_t encoding, uint64_t val)
{
	vmwrite(encoding, val);
	vmwrite(encoding+1, val >> 32);
}

void
vmcs_set_defaults(struct vmcs *vmcs, uint64_t *pml4ept, uint32_t pinbased_ctls,
		  uint32_t procbased_ctls, uint32_t procbased_ctls2,
		  uint32_t exit_ctls, uint32_t entry_ctls,
		  char *msr_bitmap, char *io_bitmap_a, char *io_bitmap_b,
		  uint16_t vpid, uint64_t cr0_ones_mask, uint64_t cr0_zeros_mask,
		  uint64_t cr4_ones_mask, uint64_t cr4_zeros_mask,
		  uintptr_t host_rip)
{
	struct pcpu *c;
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
	vmcs_write32(VMCS_PIN_BASED_CTLS, pinbased_ctls);
	KERN_DEBUG("Set primary processor-based controls to 0x%08x\n",
		   procbased_ctls);
	vmcs_write32(VMCS_PRI_PROC_BASED_CTLS, procbased_ctls);
	KERN_DEBUG("Set secondary processor-based controls to 0x%08x\n",
		   procbased_ctls2);
	vmcs_write32(VMCS_SEC_PROC_BASED_CTLS, procbased_ctls2);
	KERN_DEBUG("Set exit controls to 0x%08x\n", exit_ctls);
	vmcs_write32(VMCS_EXIT_CTLS, exit_ctls);
	KERN_DEBUG("Set entry controls to 0x%08x\n", entry_ctls);
	vmcs_write32(VMCS_ENTRY_CTLS, entry_ctls);

	/*
	 * Load host state. (Intel Software Developer's Manual Vol 3, Sec 24.5)
	 */
	/* host rip */
	vmcs_write32(VMCS_HOST_RIP, host_rip);

	/* host control registers */
	vmcs_write32(VMCS_HOST_CR0, rcr0());
	vmcs_write32(VMCS_HOST_CR3, rcr3());
	vmcs_write32(VMCS_HOST_CR4, rcr4());

	/* host segment selectors */
	vmcs_write16(VMCS_HOST_ES_SELECTOR, CPU_GDT_KDATA);
	vmcs_write16(VMCS_HOST_CS_SELECTOR, CPU_GDT_KCODE);
	vmcs_write16(VMCS_HOST_SS_SELECTOR, CPU_GDT_KDATA);
	vmcs_write16(VMCS_HOST_DS_SELECTOR, CPU_GDT_KDATA);
	vmcs_write16(VMCS_HOST_FS_SELECTOR, CPU_GDT_KDATA);
	vmcs_write16(VMCS_HOST_GS_SELECTOR, CPU_GDT_KDATA);
	vmcs_write16(VMCS_HOST_TR_SELECTOR, CPU_GDT_TSS);

	/* host segment base address */
	vmcs_write32(VMCS_HOST_FS_BASE, 0);
	vmcs_write32(VMCS_HOST_GS_BASE, 0);
	vmcs_write32(VMCS_HOST_TR_BASE, (uintptr_t) &c->kstack->tss);
	vmcs_write32(VMCS_HOST_GDTR_BASE, (uintptr_t) &c->kstack->gdt);
	vmcs_write32(VMCS_HOST_IDTR_BASE, (uintptr_t) idt);

	/* host control registers */
	vmcs_write32(VMCS_HOST_IA32_SYSENTER_CS, rdmsr(MSR_IA32_SYSENTER_CS));
	vmcs_write32(VMCS_HOST_IA32_SYSENTER_ESP, rdmsr(MSR_IA32_SYSENTER_ESP));
	vmcs_write32(VMCS_HOST_IA32_SYSENTER_EIP, rdmsr(MSR_IA32_SYSENTER_EIP));
	vmcs_write64(VMCS_HOST_IA32_PERF_GLOBAL_CTRL,
		     rdmsr(MSR_IA32_PERF_GLOBAL_CTRL));
	vmcs_write64(VMCS_HOST_IA32_PAT, rdmsr(MSR_PAT));
	vmcs_write64(VMCS_HOST_IA32_EFER, rdmsr(MSR_EFER));

	/*
	 * Load guest state. (Intel Software Developer's Manual Vol3, Sec 24.4,
	 * Sec 9.1)
	 */
	/* guest control registers */
	vmcs_write32(VMCS_GUEST_CR0, 0x60000010 | CR0_NE);
	vmcs_write32(VMCS_GUEST_CR3, 0);
	vmcs_write32(VMCS_GUEST_CR4, CR4_VMXE);

	/* guest debug registers */
	vmcs_write32(VMCS_GUEST_DR7, 0x00000400);

	/* guest MSRs */
	vmcs_write64(VMCS_GUEST_IA32_PAT, 0x7040600070406ULL);
	vmcs_write32(VMCS_GUEST_IA32_SYSENTER_CS, 0);
	vmcs_write32(VMCS_GUEST_IA32_SYSENTER_ESP, 0);
	vmcs_write32(VMCS_GUEST_IA32_SYSENTER_EIP, 0);
	vmcs_write64(VMCS_GUEST_IA32_DEBUGCTL, 0);

	/* EPTP */
	vmcs_write64(VMCS_EPTP, EPTP((uintptr_t) pml4ept));

	/* VPID */
	vmcs_write16(VMCS_VPID, vpid);

	/* exception bitmap */
	/* vmcs_write32(VMCS_EXCEPTION_BITMAP, (1 << T_MCHK)); */

	/* MSR bitmap */
	vmcs_write64(VMCS_MSR_BITMAP, (uintptr_t) msr_bitmap);

	/* link pointer */
	vmcs_write64(VMCS_LINK_POINTER, 0xffffffffffffffffULL);

	/* CR0 mask & shadow */
	vmcs_write32(VMCS_CR0_MASK, cr0_ones_mask | cr0_zeros_mask);
	vmcs_write32(VMCS_CR0_SHADOW, cr0_ones_mask);

	/* CR4 mask & shadow */
	vmcs_write32(VMCS_CR4_MASK, cr4_ones_mask | cr4_zeros_mask);
	vmcs_write32(VMCS_CR4_SHADOW, cr4_ones_mask);

	/* I/O bitmap */
	vmcs_write64(VMCS_IO_BITMAP_A, (uintptr_t) io_bitmap_a);
	vmcs_write64(VMCS_IO_BITMAP_B, (uintptr_t) io_bitmap_b);

	/* others */
	vmcs_write32(VMCS_GUEST_ACTIVITY, 0);
	vmcs_write32(VMCS_GUEST_INTERRUPTIBILITY, 0);
	vmcs_write32(VMCS_GUEST_PENDING_DBG_EXCEPTIONS, 0);
	vmcs_write32(VMCS_ENTRY_INTR_INFO, 0);
}
