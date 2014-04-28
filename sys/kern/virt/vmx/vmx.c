/*
 * Derived from BHyVe (svn 237539).
 * Adapted for CertiKOS by Haozhong Zhang at Yale.
 */

/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <preinit/lib/debug.h>
#include <preinit/lib/types.h>
#include <lib/x86.h>
#include <sys/dev/tsc.h>

#include <preinit/dev/pic.h>

#include "ept.h"
#include "vmcs.h"
#include "vmx.h"
#include "vmx_controls.h"
#include "vmx_msr.h"
#include "x86.h"

#define PAGESIZE 4096

extern void *memset(void *v, unsigned int c, unsigned int n);

static int vmx_get_reg(guest_reg_t reg, uint32_t *val);
static int vmx_set_reg(guest_reg_t reg, uint32_t val);

struct vmx vmx;

/*
static struct {
	struct vmx	vmx[MAX_VMID];
	bool		used[MAX_VMID];
} vmx_pool;
*/

struct vmx_info {
	bool		vmx_enabled;
	uint32_t	pinbased_ctls;
	uint32_t	procbased_ctls, procbased_ctls2;
	uint32_t	exit_ctls, entry_ctls;
	uint64_t	cr0_ones_mask, cr0_zeros_mask;
	uint64_t	cr4_ones_mask, cr4_zeros_mask;

	void		*vmx_region;
} vmx_cpu_info;


static int
vmx_intercept_ioport(uint16_t port, bool enable)
{

#ifdef DEBUG_GUEST_IOPORT
	VMX_DEBUG("%s intercepting guest I/O port 0x%x.\n",
		  (enable == TRUE) ? "Enable" : "Disable", port);
#endif

	uint32_t *bitmap = (uint32_t *) vmx.io_bitmap;

	int entry = port / 32;
	int bit = port - entry * 32;

	if (enable == TRUE)
		bitmap[entry] |= (1 << bit);
	else
		bitmap[entry] &= ~(1 << bit);

	return 0;
}

static int
vmx_intercept_all_ioports(bool enable)
{

#ifdef DEBUG_GUEST_IOPORT
	VMX_DEBUG("%s intercepting all guest I/O ports.\n",
		  (enable == TRUE) ? "Enable" : "Disable");
#endif

	uint32_t *bitmap = (uint32_t *) vmx.io_bitmap;

	if (enable == TRUE)
		memset(bitmap, 0xff, PAGESIZE * 2);
	else
		memzero(bitmap, PAGESIZE * 2);


	return 0;
}

static void
vmx_intercept_msr_helper(uint32_t msr, bool write, bool enable)
{
	uint32_t *msr_bitmap;
	uint32_t offset;
	int entry, bit;

	msr_bitmap = (uint32_t *)
		((uintptr_t) vmx.msr_bitmap + ((write == TRUE) ? 0 : 2048));

	if (msr <= 0x00001fff) {
		offset = msr - 0x00000000;
	} else {
		msr_bitmap = (uint32_t *) ((uintptr_t) msr_bitmap + 1024);
		offset = msr - 0xc0000000;
	}

	entry = offset / 32;
	bit = offset - entry * 32;

	if (enable == TRUE)
		msr_bitmap[entry] |= (1 << bit);
	else
		msr_bitmap[entry] &= ~(1 << bit);
}

static int
vmx_intercept_msr(uint32_t msr, int rw)
{

#ifdef DEBUG_GUEST_MSR
	VMX_DEBUG("%s intercepting rdmsr 0x%08x, "
		  "%s intercepting wrmsr 0x%08x.\n",
		  (rw & 0x1) ? "Enable" : "Disable", msr,
		  (rw & 0x2) ? "Enable" : "Disable", msr);
#endif

	if (!((msr <= 0x00001fff) ||
	      (0xc0000000 <= msr && msr <= 0xc0001fff))) {
#ifdef DEBUG_GUEST_MSR
		VMX_DEBUG("MSR 0x%08x out of range.\n", msr);
#endif
		return 1;
	}

	vmx_intercept_msr_helper(msr, FALSE, (rw & 0x1) ? TRUE : FALSE);
	vmx_intercept_msr_helper(msr, TRUE, (rw & 0x2) ? TRUE : FALSE);

	return 0;
}

static int
vmx_intercept_all_msrs(int rw)
{

#ifdef DEBUG_GUEST_MSR
	VMX_DEBUG("%s intercepting reading all guest MSRs, "
		  "%s intercepting writing all guest MSRs.\n",
		  (rw & 0x1) ? "Enable" : "Disable",
		  (rw & 0x2) ? "Enable" : "Disable");
#endif

	char *msr_bitmap = vmx.msr_bitmap;
	char *rdmsr_bitmap = msr_bitmap;
	char *wrmsr_bitmap = (char *) ((uintptr_t) msr_bitmap + 0x800);

	memset(rdmsr_bitmap, (rw & 0x1) ? 0xf : 0x0, 2048);
	memset(wrmsr_bitmap, (rw & 0x2) ? 0xf : 0x0, 2048);

	return 0;
}

static int
vmx_intercept_intr_window(bool enable)
{
	uint32_t procbased_ctls = vmcs_read32(VMCS_PRI_PROC_BASED_CTLS);
	if (enable == TRUE)
		procbased_ctls |= PROCBASED_INT_WINDOW_EXITING;
	else
		procbased_ctls &= ~PROCBASED_INT_WINDOW_EXITING;
	vmcs_write32(VMCS_PRI_PROC_BASED_CTLS, procbased_ctls);

	return 0;
}

static int
vmx_get_reg(guest_reg_t reg, uint32_t *val)
{
	KERN_ASSERT(val != NULL);

	switch (reg) {
	case GUEST_EAX:
		*val = (uint32_t) vmx.g_rax;
		break;
	case GUEST_EBX:
		*val = (uint32_t) vmx.g_rbx;
		break;
	case GUEST_ECX:
		*val = (uint32_t) vmx.g_rcx;
		break;
	case GUEST_EDX:
		*val = (uint32_t) vmx.g_rdx;
		break;
	case GUEST_ESI:
		*val = (uint32_t) vmx.g_rsi;
		break;
	case GUEST_EDI:
		*val = (uint32_t) vmx.g_rdi;
		break;
	case GUEST_EBP:
		*val = (uint32_t) vmx.g_rbp;
		break;
	case GUEST_ESP:
		*val = vmcs_read32(VMCS_GUEST_RSP);
		break;
	case GUEST_EIP:
		*val = (uint32_t) vmx.g_rip;
		break;
	case GUEST_EFLAGS:
		*val = vmcs_read32(VMCS_GUEST_RFLAGS);
		break;
	case GUEST_CR0:
		*val = vmcs_read32(VMCS_GUEST_CR0);
		break;
	case GUEST_CR2:
		*val = (uint32_t) vmx.g_cr2;
		break;
	case GUEST_CR3:
		*val = vmcs_read32(VMCS_GUEST_CR3);
		break;
	case GUEST_CR4:
		*val = vmcs_read32(VMCS_GUEST_CR4);
		break;
	default:
		return 1;
	}

	return 0;
}

static int
vmx_set_reg(guest_reg_t reg, uint32_t val)
{
	switch (reg) {
	case GUEST_EAX:
		vmx.g_rax = val;
		break;
	case GUEST_EBX:
		vmx.g_rbx = val;
		break;
	case GUEST_ECX:
		vmx.g_rcx = val;
		break;
	case GUEST_EDX:
		vmx.g_rdx = val;
		break;
	case GUEST_ESI:
		vmx.g_rsi = val;
		break;
	case GUEST_EDI:
		vmx.g_rdi = val;
		break;
	case GUEST_EBP:
		vmx.g_rbp = val;
		break;
	case GUEST_ESP:
		vmcs_write32(VMCS_GUEST_RSP, val);
		break;
	case GUEST_EIP:
		vmx.g_rip = val;
		break;
	case GUEST_EFLAGS:
		vmcs_write32(VMCS_GUEST_RFLAGS, val);
		break;
	case GUEST_CR0:
		vmcs_write32(VMCS_GUEST_CR0, val);
		break;
	case GUEST_CR2:
		vmx.g_cr2 = val;
		break;
	case GUEST_CR3:
		vmcs_write32(VMCS_GUEST_CR3, val);
		break;
	case GUEST_CR4:
		vmcs_write32(VMCS_GUEST_CR4, val);
		break;
	default:
		return 1;
	}

	return 0;
}

static int
vmx_get_msr(uint32_t msr, uint64_t *val)
{
	KERN_ASSERT(val != NULL);

	if (!(msr <= 0x00001fff || (0xc0000000 <= msr && msr <= 0xc0001fff)))
		return 1;

	*val = rdmsr(msr);

#ifdef DEBUG_GUEST_MSR
	VMX_DEBUG("Guest rdmsr 0x%08x = 0x%llx.\n", msr, *val);
#endif

	return 0;
}

static int
vmx_set_msr(uint32_t msr, uint64_t val)
{
	if (!(msr <= 0x00001fff || (0xc0000000 <= msr && msr <= 0xc0001fff)))
		return 1;

	wrmsr(msr, val);

#ifdef DEBUG_GUEST_MSR
	VMX_DEBUG("Guest wrmsr 0x%08x, 0x%llx.\n", msr, val);
#endif

	return 0;
}

static struct {
	uint32_t selector;
	uint32_t base;
	uint32_t limit;
	uint32_t access;
} seg_encode[10] = {
	[GUEST_CS] = { VMCS_GUEST_CS_SELECTOR, VMCS_GUEST_CS_BASE,
		       VMCS_GUEST_CS_LIMIT, VMCS_GUEST_CS_ACCESS_RIGHTS },
	[GUEST_SS] = { VMCS_GUEST_SS_SELECTOR, VMCS_GUEST_SS_BASE,
		       VMCS_GUEST_SS_LIMIT, VMCS_GUEST_SS_ACCESS_RIGHTS },
	[GUEST_DS] = { VMCS_GUEST_DS_SELECTOR, VMCS_GUEST_DS_BASE,
		       VMCS_GUEST_DS_LIMIT, VMCS_GUEST_DS_ACCESS_RIGHTS },
	[GUEST_ES] = { VMCS_GUEST_ES_SELECTOR, VMCS_GUEST_ES_BASE,
		       VMCS_GUEST_ES_LIMIT, VMCS_GUEST_ES_ACCESS_RIGHTS },
	[GUEST_FS] = { VMCS_GUEST_FS_SELECTOR, VMCS_GUEST_FS_BASE,
		       VMCS_GUEST_FS_LIMIT, VMCS_GUEST_FS_ACCESS_RIGHTS },
	[GUEST_GS] = { VMCS_GUEST_GS_SELECTOR, VMCS_GUEST_GS_BASE,
		       VMCS_GUEST_GS_LIMIT, VMCS_GUEST_GS_ACCESS_RIGHTS },
	[GUEST_LDTR] = { VMCS_GUEST_LDTR_SELECTOR, VMCS_GUEST_LDTR_BASE,
			 VMCS_GUEST_LDTR_LIMIT, VMCS_GUEST_LDTR_ACCESS_RIGHTS },
	[GUEST_TR] = { VMCS_GUEST_TR_SELECTOR, VMCS_GUEST_TR_BASE,
		       VMCS_GUEST_TR_LIMIT, VMCS_GUEST_TR_ACCESS_RIGHTS },
	[GUEST_GDTR] = { 0, VMCS_GUEST_GDTR_BASE, VMCS_GUEST_GDTR_LIMIT, 0 },
	[GUEST_IDTR] = { 0, VMCS_GUEST_IDTR_BASE, VMCS_GUEST_IDTR_LIMIT, 0 },
};

static int
vmx_get_desc(guest_seg_t seg, struct guest_seg_desc *desc)
{
	KERN_ASSERT(desc != NULL);

	desc->base = vmcs_read32(seg_encode[seg].base);
	desc->lim = vmcs_read32(seg_encode[seg].limit);

	if (seg != GUEST_GDTR && seg != GUEST_IDTR) {
		desc->ar = vmcs_read32(seg_encode[seg].access);
		desc->sel = vmcs_read32(seg_encode[seg].selector);
	}

	return 0;
}

static int
vmx_set_desc(guest_seg_t seg, struct guest_seg_desc *desc)
{
	KERN_ASSERT(desc != NULL);

	vmcs_write32(seg_encode[seg].base, desc->base);
	vmcs_write32(seg_encode[seg].limit, desc->lim);

	if (seg != GUEST_GDTR && seg != GUEST_IDTR) {
		vmcs_write32(seg_encode[seg].access, desc->ar);
		vmcs_write32(seg_encode[seg].selector, desc->sel);
	}

	return 0;
}

static int
vmx_get_mmap(uintptr_t gpa, uintptr_t *hpa)
{
	KERN_ASSERT(hpa != NULL);

	if (gpa % PAGESIZE)
		return 1;

	*hpa = ept_gpa_to_hpa(gpa);
	return 0;
}

static int
vmx_set_mmap(uintptr_t gpa, uint64_t hpa, uint8_t mem_type)
{
	int rc = 0;

	rc = ept_add_mapping(gpa, hpa, mem_type);

	if (rc) {
		KERN_DEBUG("EPT_add mapping error : ept@0x%x, gpa:0x%x, "
			   "hpa:0x%llx\n", vmx.pml4ept, gpa, hpa);
		return 1;
	} else {
		ept_invalidate_mappings((uint64_t)(uintptr_t) vmx.pml4ept);
	}

	return 0;
}

static int
vmx_inject_event(guest_event_t type,
		 uint8_t vector, uint32_t errcode, bool ev)
{
	uint32_t intr_info = vmcs_read32(VMCS_ENTRY_INTR_INFO);
	if (intr_info & VMCS_INTERRUPTION_INFO_VALID)
		return 1;

	intr_info = VMCS_INTERRUPTION_INFO_VALID |
		type | vector | ((ev == TRUE ? 1 : 0) << 11);
	vmcs_write32(VMCS_ENTRY_INTR_INFO, intr_info);
	if (ev == TRUE)
		vmcs_write32(VMCS_ENTRY_EXCEPTION_ERROR, errcode);

	return 0;
}

static int
vmx_get_next_eip(instr_t instr, uint32_t *val)
{
	KERN_ASSERT(val != NULL);
	*val = vmx.g_rip + vmcs_read32(VMCS_EXIT_INSTRUCTION_LENGTH);
	return 0;
}

static bool
vmx_pending_event()
{
	return (vmcs_read32(VMCS_ENTRY_INTR_INFO) &
		VMCS_INTERRUPTION_INFO_VALID) ? TRUE : FALSE;
}

static bool
vmx_intr_shadow()
{
	return (vmcs_read32(VMCS_GUEST_INTERRUPTIBILITY) &
		(VMCS_INTERRUPTIBILITY_STI_BLOCKING |
		 VMCS_INTERRUPTIBILITY_MOVSS_BLOCKING)) ? TRUE : FALSE;
}

static void
vmx_set_tsc_offset(uint64_t tsc_offset) 
{
	 vmcs_write64(VMCS_TSC_OFFSET, tsc_offset); 
}

static uint64_t
vmx_get_tsc_offset() 
{
	 return vmcs_read64(VMCS_TSC_OFFSET); 
}

