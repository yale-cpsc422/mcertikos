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

#include <sys/debug.h>
#include <sys/mem.h>
#include <sys/pcpu.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/pic.h>

#include "ept.h"
#include "vmcs.h"
#include "vmx.h"
#include "vmx_controls.h"
#include "vmx_msr.h"
#include "x86.h"

static struct {
	struct vmx	vmx[MAX_VMID];
	bool		used[MAX_VMID];
} vmx_pool;

static spinlock_t vmx_pool_lock;

static struct vmx *
alloc_vmx(void)
{
	int i;
	struct vmx *vmx = NULL;

	spinlock_acquire(&vmx_pool_lock);

	for (i = 0; i < MAX_VMID; i++)
		if (vmx_pool.used[i] == FALSE)
			break;

	if (i < MAX_VMID) {
		vmx_pool.used[i] = TRUE;
		vmx = &vmx_pool.vmx[i];
		memzero(vmx, sizeof(struct vmx));
	}

	spinlock_release(&vmx_pool_lock);

	return vmx;
}

static void
free_vmx(struct vmx *vmx)
{
	uintptr_t offset;

	if (vmx < vmx_pool.vmx)
		return;

	offset = (uintptr_t) vmx - (uintptr_t) &vmx_pool.vmx;
	if (offset % sizeof(struct vmx) ||
	    offset / sizeof(struct vmx) >= MAX_VMID)
		return;

	spinlock_acquire(&vmx_pool_lock);
	vmx_pool.used[vmx - vmx_pool.vmx] = FALSE;
	spinlock_release(&vmx_pool_lock);
}

#define PINBASED_CTLS_ONE_SETTING		\
	PINBASED_EXTINT_EXITING/*	|	\ */
	/* PINBASED_NMI_EXITING		|	\ */
	/* PINBASED_VIRTUAL_NMI */
#define PINBASED_CTLS_ZERO_SETTING	0

#define PROCBASED_CTLS_WINDOW_SETTING		\
	PROCBASED_INT_WINDOW_EXITING/* 	|	\ */
	/* PROCBASED_NMI_WINDOW_EXITING */

#define PROCBASED_CTLS_ONE_SETTING		\
	(PROCBASED_IO_BITMAPS		|	\
	 PROCBASED_MSR_BITMAPS		|	\
	 PROCBASED_CTLS_WINDOW_SETTING	|	\
	 PROCBASED_SECONDARY_CONTROLS	|	\
	 PROCBASED_RDTSC_EXITING	|	\
	 /* unsupported instructions */		\
	 PROCBASED_HLT_EXITING		|	\
	 PROCBASED_MWAIT_EXITING	|	\
	 PROCBASED_MONITOR_EXITING)
#define PROCBASED_CTLS_ZERO_SETTING		\
	(PROCBASED_CR3_LOAD_EXITING	|	\
	 PROCBASED_CR3_STORE_EXITING	|	\
	 PROCBASED_CR8_LOAD_EXITING	|	\
	 PROCBASED_CR8_STORE_EXITING	|	\
	 PROCBASED_USE_TPR_SHADOW	|	\
	 PROCBASED_MOV_DR_EXITING	|	\
	 PROCBASED_MTF)

#define PROCBASED_CTLS2_ONE_SETTING		\
	(PROCBASED2_ENABLE_EPT		|	\
	 PROCBASED2_ENABLE_VPID		|	\
	 PROCBASED2_UNRESTRICTED_GUEST	|	\
	 /* unsupported instructions */		\
	 PROCBASED2_WBINVD_EXITING)
#define PROCBASED_CTLS2_ZERO_SETTING		\
	(PROCBASED2_VIRTUALIZE_APIC	|	\
	 PROCBASED2_DESC_TABLE_EXITING	|	\
	 PROCBASED2_VIRTUALIZE_X2APIC	|	\
	 PROCBASED2_PAUSE_LOOP_EXITING	|	\
	 PROCBASED2_RDRAND_EXITING	|	\
	 PROCBASED2_ENABLE_INVPCID	|	\
	 PROCBASED2_ENABLE_RDTSCP	|	\
	 PROCBASED2_ENABLE_VMFUNC)

#define VM_EXIT_CTLS_ONE_SETTING		\
	(VM_EXIT_SAVE_DEBUG_CONTROLS	|	\
	 VM_EXIT_SAVE_PAT		|	\
	 VM_EXIT_LOAD_PAT		|	\
	 VM_EXIT_SAVE_EFER		|	\
	 VM_EXIT_LOAD_EFER)
#define VM_EXIT_CTLS_ZERO_SETTING		\
	(VM_EXIT_HOST_LMA		|	\
	 VM_EXIT_LOAD_PERF_GLOBAL_CTRL	|	\
	 VM_EXIT_ACKNOWLEDGE_INTERRUPT	|	\
	 VM_EXIT_SAVE_PREEMPTION_TIMER)

#define VM_ENTRY_CTLS_ONE_SETTING		\
	(VM_ENTRY_LOAD_DEBUG_CONTROLS	|	\
	 VM_ENTRY_LOAD_PAT		|	\
	 VM_ENTRY_LOAD_EFER)
#define VM_ENTRY_CTLS_ZERO_SETTING		\
	(VM_ENTRY_GUEST_LMA		|	\
	 VM_ENTRY_INTO_SMM		|	\
	 VM_ENTRY_DEACTIVATE_DUAL_MONITOR |	\
	 VM_ENTRY_LOAD_PERF_GLOBAL_CTRL)

struct vmx_info {
	bool		vmx_enabled;
	uint32_t	pinbased_ctls;
	uint32_t	procbased_ctls, procbased_ctls2;
	uint32_t	exit_ctls, entry_ctls;
	uint64_t	cr0_ones_mask, cr0_zeros_mask;
	uint64_t	cr4_ones_mask, cr4_zeros_mask;

	void		*vmx_region;
} vmx_cpu_info[MAX_CPU];

#if defined (DEBUG_VMEXIT)

static char *exit_reason_string[60] = {
	[EXIT_REASON_EXCEPTION] = "Exception/NMI",
	[EXIT_REASON_EXT_INTR] = "ExtINTR",
	[EXIT_REASON_TRIPLE_FAULT] = "Triple Fault",
	[EXIT_REASON_INIT] = "INIT Signal",
	[EXIT_REASON_SIPI] = "SIPI Signal",
	[EXIT_REASON_IO_SMI] = "SMI",
	[EXIT_REASON_SMI] = "Other SMI",
	[EXIT_REASON_INTR_WINDOW] = "INTR Window",
	[EXIT_REASON_NMI_WINDOW] = "NMI Window",
	[EXIT_REASON_TASK_SWITCH] = "Task Switch",
	[EXIT_REASON_CPUID] = "CPUID",
	[EXIT_REASON_GETSEC] = "GETSEC",
	[EXIT_REASON_HLT] = "HLT",
	[EXIT_REASON_INVD] = "INVD",
	[EXIT_REASON_INVLPG] = "INVLPG",
	[EXIT_REASON_RDPMC] = "RDPMC",
	[EXIT_REASON_RDTSC] = "RDTSC",
	[EXIT_REASON_RSM] = "RSM",
	[EXIT_REASON_VMCALL] = "VMCALL",
	[EXIT_REASON_VMCLEAR] = "VMCLEAR",
	[EXIT_REASON_VMLAUNCH] = "VMLAUNCH",
	[EXIT_REASON_VMPTRLD] = "VMPTRLD",
	[EXIT_REASON_VMPTRST] = "VMPTRST",
	[EXIT_REASON_VMRESUME] = "VMRESUME",
	[EXIT_REASON_VMWRITE] = "VMWRITE",
	[EXIT_REASON_VMREAD] = "VMREAD",
	[EXIT_REASON_VMXON] = "VMXON",
	[EXIT_REASON_VMXOFF] = "VMXOFF",
	[EXIT_REASON_CR_ACCESS] = "CR Access",
	[EXIT_REASON_DR_ACCESS] = "DR Access",
	[EXIT_REASON_INOUT] = "I/O",
	[EXIT_REASON_RDMSR] = "RDMSR",
	[EXIT_REASON_WRMSR] = "WRMSR",
	[EXIT_REASON_INVAL_VMCS] = "Invalid Guest State",
	[EXIT_REASON_INVAL_MSR] = "Invalid Guest MSRs",
	[EXIT_REASON_MWAIT] = "MWAIT",
	[EXIT_REASON_MTF] = "Monitor Trap Flag",
	[EXIT_REASON_MONITOR] = "MONITOR",
	[EXIT_REASON_PAUSE] = "PAUSE",
	[EXIT_REASON_MCE] = "MCE",
	[EXIT_REASON_TPR] = "TPR",
	[EXIT_REASON_APIC] = "APIC",
	[EXIT_REASON_GDTR_IDTR] = "GDTR/IDTR",
	[EXIT_REASON_LDTR_TR] = "LDTR/TR",
	[EXIT_REASON_EPT_FAULT] = "EPT Voilation",
	[EXIT_REASON_EPT_MISCONFIG] = "EPT Misconfiguration",
	[EXIT_REASON_INVEPT] = "INVEPT",
	[EXIT_REASON_RDTSCP] = "RDTSCP",
	[EXIT_REASON_VMX_PREEMPT] = "VMX-preemption Timer",
	[EXIT_REASON_INVVPID] = "INVVPID",
	[EXIT_REASON_WBINVD] = "WBINVD",
	[EXIT_REASON_XSETBV] = "XSETBV",
	[EXIT_REASON_RDRAND] = "RDRAND",
	[EXIT_REASON_INVPCID] = "INVPCID",
	[EXIT_REASON_VMFUNC] = "VMFUNC",
};

#endif

static void
vmx_dump_host_info(void)
{
	dprintf("Host:\n"
		"    eax 0x%08x    ebx 0x%08x    ecx 0x%08x    edx 0x%08x\n"
		"    esi 0x%08x    edi 0x%08x    ebp 0x%08x    esp 0x%08x\n"
		"    eflags 0x%08x\n",
		read_eax(), read_ebx(), read_ecx(), read_edx(),
		read_esi(), read_edi(), read_ebp(),
		/* (uintptr_t) vmcs_read(VMCS_HOST_RSP), */
		read_esp(),
		read_eflags());
}

static void
vmx_dump_info(struct vmx *vmx)
{
	KERN_ASSERT(vmx != NULL);

	uintptr_t vmcs_ptr;

	vmptrst(&vmcs_ptr);
	KERN_DEBUG("VMCS @ 0x%08x\n", vmcs_ptr);
	KERN_ASSERT(vmcs_ptr == (uintptr_t) vmx->vmcs);
	vmptrld(vmx->vmcs);

	dprintf("Guest:\n"
		"    eax 0x%08x    ebx 0x%08x    ecx 0x%08x    edx 0x%08x\n"
		"    esi 0x%08x    edi 0x%08x    ebp 0x%08x    esp 0x%08x\n"
		"    eip 0x%08x    eflags 0x%08x\n"
		"    CR0 0x%08x    CR2 0x%08x    CR3 0x%08x    CR4 0x%08x\n"
		"    DR0 0x%08x    DR1 0x%08x    DR2 0x%08x    DR3 0x%08x\n"
		"    DR6 0x%08x    DR7 0x%08x\n"
		"    CS   sel=0x%04x base=0x%08x limit=0x%04x attr=0x%08x\n"
		"    DS   sel=0x%04x base=0x%08x limit=0x%04x attr=0x%08x\n"
		"    ES   sel=0x%04x base=0x%08x limit=0x%04x attr=0x%08x\n"
		"    FS   sel=0x%04x base=0x%08x limit=0x%04x attr=0x%08x\n"
		"    GS   sel=0x%04x base=0x%08x limit=0x%04x attr=0x%08x\n"
		"    SS   sel=0x%04x base=0x%08x limit=0x%04x attr=0x%08x\n"
		"    LDTR sel=0x%04x base=0x%08x limit=0x%04x attr=0x%08x\n"
		"    TR   sel=0x%04x base=0x%08x limit=0x%04x attr=0x%08x\n"
		"    GDTR            base=0x%08x limit=0x%04x\n"
		"    IDTR            base=0x%08x limit=0x%04x\n"
		"    VPID %d         EPT 0x%llx\n"
		"Host:\n"
		"    eax 0x%08x    ebx 0x%08x    ecx 0x%08x    edx 0x%08x\n"
		"    esi 0x%08x    edi 0x%08x    ebp 0x%08x    esp 0x%08x\n"
		"    eip 0x%08x    eflags 0x%08x\n"
		"    CR0 0x%08x    CR3 0x%08x    CR4 0x%08x\n"
		"    CS   sel=0x%04x\n"
		"    DS   sel=0x%04x\n"
		"    ES   sel=0x%04x\n"
		"    FS   sel=0x%04x base=0x%08x\n"
		"    GS   sel=0x%04x base=0x%08x\n"
		"    SS   sel=0x%04x\n"
		"    TR   sel=0x%04x base=0x%08x\n"
		"    GDTR            base=0x%08x\n"
		"    IDTR            base=0x%08x\n"
		"    MSR_IA32_PAT  0x%llx\n"
		"    MSR_IA32_EFER 0x%llx\n",
		(uintptr_t) vmx->g_rax, (uintptr_t) vmx->g_rbx,
		(uintptr_t) vmx->g_rcx, (uintptr_t) vmx->g_rdx,
		(uintptr_t) vmx->g_rsi, (uintptr_t) vmx->g_rdi,
		(uintptr_t) vmx->g_rbp, (uintptr_t) vmcs_read32(VMCS_GUEST_RSP),
		(uintptr_t) vmcs_read32(VMCS_GUEST_RIP),
		(uintptr_t) vmcs_read32(VMCS_GUEST_RFLAGS),
		(uintptr_t) vmcs_read32(VMCS_GUEST_CR0),
		(uintptr_t) vmx->g_cr2,
		(uintptr_t) vmcs_read32(VMCS_GUEST_CR3),
		(uintptr_t) vmcs_read32(VMCS_GUEST_CR4),
		(uintptr_t) vmx->g_dr0, (uintptr_t) vmx->g_dr1,
		(uintptr_t) vmx->g_dr2, (uintptr_t) vmx->g_dr2,
		(uintptr_t) vmx->g_dr6, (uintptr_t) vmcs_read32(VMCS_GUEST_DR7),
		(uint16_t) vmcs_read16(VMCS_GUEST_CS_SELECTOR),
		(uintptr_t) vmcs_read32(VMCS_GUEST_CS_BASE),
		(uint16_t) vmcs_read32(VMCS_GUEST_CS_LIMIT),
		(uint32_t) vmcs_read32(VMCS_GUEST_CS_ACCESS_RIGHTS),
		(uint16_t) vmcs_read16(VMCS_GUEST_DS_SELECTOR),
		(uintptr_t) vmcs_read32(VMCS_GUEST_DS_BASE),
		(uint16_t) vmcs_read32(VMCS_GUEST_DS_LIMIT),
		(uint32_t) vmcs_read32(VMCS_GUEST_DS_ACCESS_RIGHTS),
		(uint16_t) vmcs_read16(VMCS_GUEST_ES_SELECTOR),
		(uintptr_t) vmcs_read32(VMCS_GUEST_ES_BASE),
		(uint16_t) vmcs_read32(VMCS_GUEST_ES_LIMIT),
		(uint32_t) vmcs_read32(VMCS_GUEST_ES_ACCESS_RIGHTS),
		(uint16_t) vmcs_read16(VMCS_GUEST_FS_SELECTOR),
		(uintptr_t) vmcs_read32(VMCS_GUEST_FS_BASE),
		(uint16_t) vmcs_read32(VMCS_GUEST_FS_LIMIT),
		(uint32_t) vmcs_read32(VMCS_GUEST_FS_ACCESS_RIGHTS),
		(uint16_t) vmcs_read16(VMCS_GUEST_GS_SELECTOR),
		(uintptr_t) vmcs_read32(VMCS_GUEST_GS_BASE),
		(uint16_t) vmcs_read32(VMCS_GUEST_GS_LIMIT),
		(uint32_t) vmcs_read32(VMCS_GUEST_GS_ACCESS_RIGHTS),
		(uint16_t) vmcs_read16(VMCS_GUEST_SS_SELECTOR),
		(uintptr_t) vmcs_read32(VMCS_GUEST_SS_BASE),
		(uint16_t) vmcs_read32(VMCS_GUEST_SS_LIMIT),
		(uint32_t) vmcs_read32(VMCS_GUEST_SS_ACCESS_RIGHTS),
		(uint16_t) vmcs_read16(VMCS_GUEST_LDTR_SELECTOR),
		(uintptr_t) vmcs_read32(VMCS_GUEST_LDTR_BASE),
		(uint16_t) vmcs_read32(VMCS_GUEST_LDTR_LIMIT),
		(uint32_t) vmcs_read32(VMCS_GUEST_LDTR_ACCESS_RIGHTS),
		(uint16_t) vmcs_read16(VMCS_GUEST_TR_SELECTOR),
		(uintptr_t) vmcs_read32(VMCS_GUEST_TR_BASE),
		(uint16_t) vmcs_read32(VMCS_GUEST_TR_LIMIT),
		(uint32_t) vmcs_read32(VMCS_GUEST_TR_ACCESS_RIGHTS),
		(uintptr_t) vmcs_read32(VMCS_GUEST_GDTR_BASE),
		(uint16_t) vmcs_read32(VMCS_GUEST_GDTR_LIMIT),
		(uintptr_t) vmcs_read32(VMCS_GUEST_IDTR_BASE),
		(uint16_t) vmcs_read32(VMCS_GUEST_IDTR_LIMIT),
		(uint32_t) vmcs_read16(VMCS_VPID),
		vmcs_read64(VMCS_EPTP),
		read_eax(), read_ebx(), read_ecx(), read_edx(),
		read_esi(), read_edi(), read_ebp(),
		/* (uintptr_t) vmcs_read(VMCS_HOST_RSP), */
		read_esp(),
		(uintptr_t) vmcs_read32(VMCS_HOST_RIP),
		read_eflags(),
		(uintptr_t) vmcs_read32(VMCS_HOST_CR0),
		(uintptr_t) vmcs_read32(VMCS_HOST_CR3),
		(uintptr_t) vmcs_read32(VMCS_HOST_CR4),
		(uint16_t) vmcs_read16(VMCS_HOST_CS_SELECTOR),
		(uint16_t) vmcs_read16(VMCS_HOST_DS_SELECTOR),
		(uint16_t) vmcs_read16(VMCS_HOST_ES_SELECTOR),
		(uint16_t) vmcs_read16(VMCS_HOST_FS_SELECTOR),
		(uintptr_t) vmcs_read32(VMCS_HOST_FS_BASE),
		(uint16_t) vmcs_read16(VMCS_HOST_GS_SELECTOR),
		(uintptr_t) vmcs_read32(VMCS_HOST_GS_BASE),
		(uint16_t) vmcs_read16(VMCS_HOST_SS_SELECTOR),
		(uint16_t) vmcs_read16(VMCS_HOST_TR_SELECTOR),
		(uintptr_t) vmcs_read32(VMCS_HOST_TR_BASE),
		(uintptr_t) vmcs_read32(VMCS_HOST_GDTR_BASE),
		(uintptr_t) vmcs_read32(VMCS_HOST_IDTR_BASE),
		vmcs_read64(VMCS_HOST_IA32_PAT),
		vmcs_read64(VMCS_HOST_IA32_EFER));
}

static int
vmx_enable(void)
{
	pageinfo_t *pi;
	struct vmx_info *vmx_info;
	int error;

	vmx_info = &vmx_cpu_info[pcpu_cpu_idx(pcpu_cur())];

	lcr4(rcr4() | CR4_VMXE);

	if ((pi = mem_page_alloc()) == NULL) {
		KERN_DEBUG("Cannot allocate memory for VMX regison.\n");
		return 1;
	}
	vmx_info->vmx_region = (void *) mem_pi2phys(pi);
	memzero(vmx_info->vmx_region, PAGESIZE);

	*(uint32_t *) vmx_info->vmx_region = rdmsr(MSR_VMX_BASIC) & 0xffffffff;

	error = vmxon(vmx_info->vmx_region);
	if (!error)
		vmx_info->vmx_enabled = TRUE;
	else
		vmx_info->vmx_enabled = FALSE;

	return error;
}

static struct vmcs *
alloc_vmcs(void)
{
	pageinfo_t *pi;
	struct vmcs *vmcs;

	if ((pi = mem_page_alloc()) ==NULL)
		return NULL;

	vmcs = mem_pi2ptr(pi);
	memzero(vmcs, sizeof(struct vmcs));

	return vmcs;
}

static void
free_vmcs(struct vmcs *vmcs)
{
	if (vmcs == NULL)
		return;
	mem_page_free(mem_ptr2pi(vmcs));
}

static uint64_t *
alloc_pml4(void)
{
	pageinfo_t *pi;
	uint64_t *pml4;

	if ((pi = mem_page_alloc()) == NULL)
		return NULL;

	pml4 = mem_pi2ptr(pi);
	memzero(pml4, PAGESIZE);

	return pml4;
}

static void
free_pml4(uint64_t *pml4)
{
	if (pml4 == NULL)
		return;
	mem_page_free(mem_ptr2pi(pml4));
}

static char *
alloc_bitmap(size_t size)
{
	pageinfo_t *pi;
	char *bitmap;

	if (size == 0 || size % PAGESIZE)
		return NULL;

	if ((pi = mem_pages_alloc(size)) == NULL)
		return NULL;

	bitmap = mem_pi2ptr(pi);
	memzero(bitmap, size);

	return bitmap;
}

static void
free_bitmap(void *bitmap)
{
	if (bitmap == NULL)
		return;
	mem_page_free(bitmap);
}

static int
vmx_handle_exit(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx = (struct vmx *) vm->cookie;

	vm->exit_handled = FALSE;

	switch (vmx->exit_reason & EXIT_REASON_MASK) {
	case EXIT_REASON_EXT_INTR:
		vm->exit_reason = EXIT_FOR_EXTINT;
		break;

	case EXIT_REASON_INTR_WINDOW:
		vm->exit_reason = EXIT_FOR_INTWIN;
		break;

	case EXIT_REASON_INOUT:
		vm->exit_reason = EXIT_FOR_IOPORT;
		vm->exit_info.ioport.port =
			EXIT_QUAL_IO_PORT(vmx->exit_qualification);
		if (EXIT_QUAL_IO_SIZE(vmx->exit_qualification) ==
		    EXIT_QUAL_IO_ONE_BYTE)
			vm->exit_info.ioport.width = SZ8;
		else if (EXIT_QUAL_IO_SIZE(vmx->exit_qualification) ==
			 EXIT_QUAL_IO_TWO_BYTE)
			vm->exit_info.ioport.width = SZ16;
		else
			vm->exit_info.ioport.width = SZ32;
		if (EXIT_QUAL_IO_DIR(vmx->exit_qualification) ==
		    EXIT_QUAL_IO_IN)
			vm->exit_info.ioport.write = FALSE;
		else
			vm->exit_info.ioport.write = TRUE;
		vm->exit_info.ioport.rep =
			EXIT_QUAL_IO_REP(vmx->exit_qualification) ? TRUE : FALSE;
		vm->exit_info.ioport.str =
			EXIT_QUAL_IO_STR(vmx->exit_qualification) ? TRUE : FALSE;
		break;

	case EXIT_REASON_EPT_FAULT:
		vm->exit_reason = EXIT_FOR_PGFLT;
		vm->exit_info.pgflt.addr =
			(uintptr_t) vmcs_read64(VMCS_GUEST_PHYSICAL_ADDRESS);
		break;

	case EXIT_REASON_CPUID:
		vm->exit_reason = EXIT_FOR_CPUID;
		break;

	case EXIT_REASON_RDTSC:
		vm->exit_reason = EXIT_FOR_RDTSC;
		break;

	case EXIT_REASON_RDMSR:
		vm->exit_reason = EXIT_FOR_RDMSR;
		break;

	case EXIT_REASON_WRMSR:
		vm->exit_reason = EXIT_FOR_WRMSR;
		break;

	case EXIT_REASON_VMCALL:
		vm->exit_reason = EXIT_FOR_HYPERCALL;
		break;

	case EXIT_REASON_RDTSCP:
	case EXIT_REASON_HLT:
	case EXIT_REASON_VMCLEAR:
	case EXIT_REASON_VMLAUNCH:
	case EXIT_REASON_VMPTRLD:
	case EXIT_REASON_VMPTRST:
	case EXIT_REASON_VMREAD:
	case EXIT_REASON_VMRESUME:
	case EXIT_REASON_VMWRITE:
	case EXIT_REASON_VMXOFF:
	case EXIT_REASON_VMXON:
	case EXIT_REASON_MWAIT:
	case EXIT_REASON_MONITOR:
		vm->exit_reason = EXIT_FOR_INVAL_INSTR;
		break;

	default:
		vm->exit_reason = EXIT_INVAL;
	}

	return 0;
}

static int
vmx_init(void)
{
	int error;
	uint32_t dummy, val;
	uint64_t fixed0, fixed1;
	struct vmx_info *vmx_info;

	KERN_ASSERT(pcpu_cur() != NULL);
	vmx_info = &vmx_cpu_info[pcpu_cpu_idx(pcpu_cur())];

	memset(&vmx_pool, 0, sizeof(vmx_pool));

	/* CPUID.1:ECX[bit 5] must be 1 for processor to support VMX */
	cpuid(0x00000001, &dummy, &dummy, &val, &dummy);
	if (!(val & CPUID_FEATURE_VMX)) {
		KERN_DEBUG("No VMX (cpuid 0x1 : ecx=0x%08x).\n", val);
		return 1;
	}

	KERN_DEBUG("MSR_VMX_BASIC = 0x%llx\n", rdmsr(MSR_VMX_BASIC));

	/* setup pin-based control registers */
	KERN_DEBUG("MSR_VMX_PINBASED_CTLS = 0x%llx\n",
		   rdmsr(MSR_VMX_PINBASED_CTLS));
	if ((rdmsr(MSR_VMX_BASIC) & (1ULL << 55)))
		KERN_DEBUG("MSR_VMX_TRUE_PINBASED_CTLS = 0x%llx\n",
			   rdmsr(MSR_VMX_TRUE_PINBASED_CTLS));
	else
		KERN_DEBUG("NO MSR_VMX_TRUE_PINBASED_CTLS\n");
	error = vmx_set_ctlreg
		(MSR_VMX_PINBASED_CTLS,     MSR_VMX_TRUE_PINBASED_CTLS,
		 PINBASED_CTLS_ONE_SETTING, PINBASED_CTLS_ZERO_SETTING,
		 &vmx_info->pinbased_ctls);
	if (error) {
		KERN_DEBUG("Not support desired pin-based controls. "
			   "(error=%d)\n", error);
		return (error);
	}
	KERN_DEBUG("pin-based ctls 0x%08x\n", vmx_info->pinbased_ctls);

	/* setup primary processor-based control registrers */
	KERN_DEBUG("MSR_VMX_PROCBASED_CTLS = 0x%llx\n",
		   rdmsr(MSR_VMX_PROCBASED_CTLS));
	if ((rdmsr(MSR_VMX_BASIC) & (1ULL << 55)))
		KERN_DEBUG("MSR_VMX_TRUE_PROCBASED_CTLS = 0x%llx\n",
			   rdmsr(MSR_VMX_TRUE_PROCBASED_CTLS));
	else
		KERN_DEBUG("NO MSR_VMX_TRUE_PROCBASED_CTLS\n");
	error = vmx_set_ctlreg
		(MSR_VMX_PROCBASED_CTLS,     MSR_VMX_TRUE_PROCBASED_CTLS,
		 PROCBASED_CTLS_ONE_SETTING, PROCBASED_CTLS_ZERO_SETTING,
		 &vmx_info->procbased_ctls);
	if (error) {
		KERN_DEBUG("Not support desired primary processor-based "
			   "controls. (error=%d)\n", error);
		return (error);
	}
	vmx_info->procbased_ctls &= ~PROCBASED_CTLS_WINDOW_SETTING;
	KERN_DEBUG("primary processor-based ctls 0x%08x\n",
		   vmx_info->procbased_ctls);

	/* setup secondary processor-based control registers */
	KERN_DEBUG("MSR_VMX_PROCBASED_CTLS2 = 0x%llx\n",
		   rdmsr(MSR_VMX_PROCBASED_CTLS2));
	error = vmx_set_ctlreg
		(MSR_VMX_PROCBASED_CTLS,      MSR_VMX_PROCBASED_CTLS2,
		 PROCBASED_CTLS2_ONE_SETTING, PROCBASED_CTLS2_ZERO_SETTING,
		 &vmx_info->procbased_ctls2);
	if (error) {
		KERN_DEBUG("Not support desired secondary processor-based "
			   "controls. (error=%d)\n", error);
		return (error);
	}
	KERN_DEBUG("secondary processor-based ctls 0x%08x\n",
		   vmx_info->procbased_ctls2);

	/* setup VM exit control registers */
	KERN_DEBUG("MSR_VMX_EXIT_CTLS = 0x%llx\n", rdmsr(MSR_VMX_EXIT_CTLS));
	if (rdmsr(MSR_VMX_BASIC) & (1ULL << 51))
		KERN_DEBUG("MSR_VMX_TRUE_EXIT_CTLS = 0x%llx\n",
			   rdmsr(MSR_VMX_TRUE_EXIT_CTLS));
	else
		KERN_DEBUG("NO MSR_VMX_TRUE_EXIT_CTLS\n");
	error = vmx_set_ctlreg
		(MSR_VMX_EXIT_CTLS,        MSR_VMX_TRUE_EXIT_CTLS,
		 VM_EXIT_CTLS_ONE_SETTING, VM_EXIT_CTLS_ZERO_SETTING,
		 &vmx_info->exit_ctls);
	if (error) {
		KERN_DEBUG("Not support desired VM-exit controls. (error=%d)\n",
			   error);
		return (error);
	}
	KERN_DEBUG("exit ctls 0x%08x\n", vmx_info->exit_ctls);

	/* setup VM entry control registers */
	KERN_DEBUG("MSR_VMX_ENTRY_CTLS = 0x%llx\n", rdmsr(MSR_VMX_ENTRY_CTLS));
	if (rdmsr(MSR_VMX_BASIC) & (1ULL << 51))
		KERN_DEBUG("MSR_VNX_TRUE_ENTRY_CTLS = 0x%llx\n",
			   rdmsr(MSR_VMX_TRUE_ENTRY_CTLS));
	else
		KERN_DEBUG("NO MSR_VMX_TRUE_ENTRY_CTLS\n");
	error = vmx_set_ctlreg
		(MSR_VMX_ENTRY_CTLS,        MSR_VMX_TRUE_ENTRY_CTLS,
		 VM_ENTRY_CTLS_ONE_SETTING, VM_ENTRY_CTLS_ZERO_SETTING,
		 &vmx_info->entry_ctls);
	if (error) {
		KERN_DEBUG("Not support desired VM-entry controls. "
			   "(error=%d)\n", error);
		return (error);
	}
	KERN_DEBUG("entry ctls 0x%08x\n", vmx_info->entry_ctls);

	/* initialize EPT */
	error = ept_init();
	if (error) {
		KERN_DEBUG("Cannot initalize EPT. (error=%d)\n", error);
		return error;
	}

	/* check fixed bits of CR0 */
	fixed0 = rdmsr(MSR_VMX_CR0_FIXED0);
	fixed1 = rdmsr(MSR_VMX_CR0_FIXED1);
	vmx_info->cr0_ones_mask = (fixed0 & fixed1) & ~(CR0_PG | CR0_PE);
	vmx_info->cr0_zeros_mask = (CR0_NW | CR0_CD) | (~fixed0 & ~fixed1);
	KERN_DEBUG("CR0 1s mask 0x%llx, 0s mask 0x%llx.\n",
		   vmx_info->cr0_ones_mask, vmx_info->cr0_zeros_mask);

	/* check fixed bits of CR4 */
	fixed0 = rdmsr(MSR_VMX_CR4_FIXED0);
	fixed1 = rdmsr(MSR_VMX_CR4_FIXED1);
	vmx_info->cr4_ones_mask = fixed0 & fixed1;
	vmx_info->cr4_zeros_mask = ~fixed0 & ~fixed1;
	KERN_DEBUG("CR4 1s mask 0x%llx, 0s mask 0x%llx.\n",
		   vmx_info->cr4_ones_mask, vmx_info->cr4_zeros_mask);

	/* enable VMX */
	error = vmx_enable();
	if (error) {
		KERN_DEBUG("Cannot enable VMX.\n");
		return error;
	}

	return 0;
}

static int
vmx_init_vm(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	extern uint8_t vmx_return_from_guest[];

	struct vmx_info *vmx_info;
	struct vmx *vmx;

	int rc = 0;

	vmx_info = &vmx_cpu_info[pcpu_cpu_idx(pcpu_cur())];
	KERN_ASSERT(vmx_info->vmx_enabled == TRUE);

	if ((vmx = alloc_vmx()) == NULL) {
		KERN_DEBUG("Cannot allocate memory for struct vmx.\n");
		rc = 1;
		goto vmx_err;
	}
	vm->cookie = vmx;

	/*
	 * Setup VMCS.
	 */
	if ((vmx->vmcs = alloc_vmcs()) == NULL) {
		KERN_DEBUG("Cannot allocate memory for VMCS.\n");
		rc = 2;
		goto vmcs_err;
	}
	KERN_DEBUG("VMCS @ 0x%08x\n", vmx->vmcs);

	/*
	 * Setup EPT.
	 */
	if ((vmx->pml4ept = alloc_pml4()) == NULL) {
		KERN_DEBUG("Cannot allocate memory for EPT.\n");
		rc = 3;
		goto pml4_err;
	}
	KERN_DEBUG("EPT @ 0x%08x.\n", vmx->pml4ept);

	if (ept_create_mappings(vmx->pml4ept, vm->memsize)) {
		KERN_DEBUG("Cannot create EPT mappings.\n");
		rc = 4;
		goto ept_err;
	}

	/*
	 * Clean up EPTP-tagged guest physical and combined mappings
	 *
	 * VMX transitions are not required to invalidate any guest physical
	 * mappings. So, it may be possible for stale guest physical mappings
	 * to be present in the processor TLBs.
	 *
	 * Combined mappings for this EP4TA are also invalidated for all VPIDs.
	 */
	ept_invalidate_mappings((uintptr_t) vmx->pml4ept);

	/*
	 * Setup MSR bitmap.
	 */
	if ((vmx->msr_bitmap = alloc_bitmap(PAGESIZE)) == NULL) {
		KERN_DEBUG("Cannot allocate memory for MSR bitmap.\n");
		rc = 5;
		goto msr_err;
	}
	KERN_DEBUG("MSR bitmap @ 0x%08x\n", vmx->msr_bitmap);
	msr_bitmap_initialize(vmx->msr_bitmap);

	/*
	 * Setup I/O bitmaps.
	 */
	if ((vmx->io_bitmap = alloc_bitmap(PAGESIZE * 2)) == NULL) {
		KERN_DEBUG("Cannot allocate memory for I/O bitmap.\n");
		rc = 6;
		goto io_err;
	}
	KERN_DEBUG("I/O bitmap A @ 0x%08x, I/O bitmap B @ 0x%08x.\n",
		   vmx->io_bitmap, (uintptr_t) vmx->io_bitmap + PAGESIZE);

	/*
	 * Setup VMCS.
	 */
	vmx->vmcs->identifier = rdmsr(MSR_VMX_BASIC) & 0xffffffff;
	vmx->g_rip = 0xfff0;
	vmcs_set_defaults
		(vmx->vmcs, vmx->pml4ept, vmx_info->pinbased_ctls,
		 vmx_info->procbased_ctls, vmx_info->procbased_ctls2,
		 vmx_info->exit_ctls, vmx_info->entry_ctls, vmx->msr_bitmap,
		 vmx->io_bitmap, (char *) ((uintptr_t) vmx->io_bitmap+PAGESIZE),
		 pcpu_cpu_idx(pcpu_cur()) + 1,
		 vmx_info->cr0_ones_mask, vmx_info->cr0_zeros_mask,
		 vmx_info->cr4_ones_mask, vmx_info->cr4_zeros_mask,
		 (uintptr_t) vmx_return_from_guest);

	vmx->vpid = pcpu_cpu_idx(pcpu_cur()) + 1;
	vmx->g_cr2 = 0;
	vmx->g_dr0 = vmx->g_dr1 = vmx->g_dr2 = vmx->g_dr3 = 0;
	vmx->g_dr6 = 0xffff0ff0;

	KERN_DEBUG("vmx_init_vm() done.\n");

	return 0;

 io_err:
	free_bitmap(vmx->msr_bitmap);
 msr_err:
	/* TODO: free EPT */
 ept_err:
	free_pml4(vmx->pml4ept);
 pml4_err:
	free_vmcs(vmx->vmcs);
 vmcs_err:
	free_vmx(vmx);
 vmx_err:
	return rc;
}

static int
vmx_run_vm(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx = (struct vmx *) vm->cookie;

	vmptrld(vmx->vmcs);

	vmcs_write32(VMCS_GUEST_RIP, vmx->g_rip);

	/* the address of vmx is stored in %ecx */
	__asm __volatile(
			 /* save host registers */
			 "pushl %%ebp;"
			 "pushl %%edi;"
			 /* save the address of vmx on the stack */
			 "pushl %%ecx;"			/* placeholder */
			 "pushl %%ecx;"			/* address of vmx */
			 "movl %1, %%edi;"
			 "vmwrite %%esp, %%edi;"
			 /* save entry TSC */
			 "pushl %%ecx;"
			 "rdtscp;"
			 "popl %%ecx;"
			 "movl %%eax, %c[enter_tsc_lo](%0);"
			 "movl %%edx, %c[enter_tsc_hi](%0);"
			 /* load guest registers */
			 "movl %c[g_cr2](%0), %%edi;"	/* guest %cr2 */
			 "movl %%edi, %%cr2;"
			 "movl %c[g_dr0](%0), %%edi;"	/* guest %dr0 */
			 "movl %%edi, %%dr0;"
			 "movl %c[g_dr1](%0), %%edi;"	/* guest %dr1 */
			 "movl %%edi, %%dr1;"
			 "movl %c[g_dr2](%0), %%edi;"	/* guest %dr2 */
			 "movl %%edi, %%dr2;"
			 "movl %c[g_dr3](%0), %%edi;"	/* guest %dr3 */
			 "movl %%edi, %%dr3;"
			 "movl %c[g_dr6](%0), %%edi;"	/* guest %dr6 */
			 /* "movl %%edi, %%dr6;" */
			 "movl %c[g_rax](%0), %%eax;"	/* guest %eax */
			 "movl %c[g_rbx](%0), %%ebx;"	/* guest %ebx */
			 "movl %c[g_rdx](%0), %%edx;"	/* guest %edx */
			 "movl %c[g_rsi](%0), %%esi;"	/* guest %esi */
			 "movl %c[g_rdi](%0), %%edi;"	/* guest %edi */
			 "movl %c[g_rbp](%0), %%ebp;"	/* guest %ebp */
			 /* launch/resume the guest */
			 "cmpl $0, %c[launched](%0); jz .Lvmx_launch;"
			 "movl %c[g_rcx](%0), %%ecx;"	/* guest %ecx */
			 "vmresume; jmp vmx_return_from_guest;"
			 ".Lvmx_launch:;"
			 "movl %c[g_rcx](%0), %%ecx;"	/* guest %ecx */
			 "vmlaunch;"
			 "vmx_return_from_guest:"
			 /* save guest registers */
			 "movl %%ecx, 4(%%esp);"	/* temporally save guest
							   %ecx in placeholder*/
			 "popl %%ecx;"			/* reload vmx */
			 /* check error first */
			 "jnc 1f;"
			 "movl $1, %c[failed](%0);"	/* CF: error = 1 */
			 "jmp 3f;"
			 "1: jnz 2f;"
			 "movl $2, %c[failed](%0);"	/* ZF: error = 2 */
			 "jmp 3f;"
			 "2: movl $0, %c[failed](%0);"
			 "3: nop;"
			 /* save guest */
			 "movl %%edi, %c[g_rdi](%0);"	/* guest %edi */
			 "movl %%cr2, %%edi;"		/* guest %cr2 */
			 "movl %%edi, %c[g_cr2](%0);"
			 "movl %%dr0, %%edi;"		/* guest %dr0 */
			 "movl %%edi, %c[g_dr0](%0);"
			 "movl %%dr1, %%edi;"		/* guest %dr1 */
			 "movl %%edi, %c[g_dr1](%0);"
			 "movl %%dr2, %%edi;"		/* guest %dr2 */
			 "movl %%edi, %c[g_dr1](%0);"
			 "movl %%dr3, %%edi;"		/* guest %dr3 */
			 "movl %%edi, %c[g_dr3](%0);"
			 "movl %%dr6, %%edi;"		/* guest %dr6 */
			 "movl %%edi, %c[g_dr6](%0);"
			 "movl %%eax, %c[g_rax](%0);"	/* guest %eax */
			 "movl %%ebx, %c[g_rbx](%0);"	/* guest %ebx */
			 "popl %%edi;"			/* guest %ecx */
			 "movl %%edi, %c[g_rcx](%0);"
			 "movl %%edx, %c[g_rdx](%0);"	/* guest %edx */
			 "movl %%esi, %c[g_rsi](%0);"	/* guest %esi */
			 "movl %%ebp, %c[g_rbp](%0);"	/* guest %ebp */
			 /* save exit TSC */
			 "pushl %%ecx;"
			 "rdtscp;"
			 "popl %%ecx;"
			 "movl %%eax, %c[exit_tsc_lo](%0);"
			 "movl %%edx, %c[exit_tsc_hi](%0);"
			 /* load host registers */
			 "popl %%edi;"
			 "popl %%ebp;"
			 : : "c" (vmx), "i" (VMCS_HOST_RSP),
			 [g_rax] "i" (offsetof(struct vmx, g_rax)),
			 [g_rbx] "i" (offsetof(struct vmx, g_rbx)),
			 [g_rcx] "i" (offsetof(struct vmx, g_rcx)),
			 [g_rdx] "i" (offsetof(struct vmx, g_rdx)),
			 [g_rsi] "i" (offsetof(struct vmx, g_rsi)),
			 [g_rdi] "i" (offsetof(struct vmx, g_rdi)),
			 [g_rbp] "i" (offsetof(struct vmx, g_rbp)),
			 [g_cr2] "i" (offsetof(struct vmx, g_cr2)),
			 [g_dr0] "i" (offsetof(struct vmx, g_dr0)),
			 [g_dr1] "i" (offsetof(struct vmx, g_dr1)),
			 [g_dr2] "i" (offsetof(struct vmx, g_dr2)),
			 [g_dr3] "i" (offsetof(struct vmx, g_dr3)),
			 [g_dr6] "i" (offsetof(struct vmx, g_dr6)),
			 [launched] "i" (offsetof(struct vmx, launched)),
			 [failed] "i" (offsetof(struct vmx, failed)),
			 [enter_tsc_lo] "i" (offsetof(struct vmx, enter_tsc[0])),
			 [enter_tsc_hi] "i" (offsetof(struct vmx, enter_tsc[1])),
			 [exit_tsc_lo] "i" (offsetof(struct vmx, exit_tsc[0])),
			 [exit_tsc_hi] "i" (offsetof(struct vmx, exit_tsc[1]))
			 : "cc", "memory", "eax", "ebx", "edx", "esi", "edi");

	if (unlikely(vmx->failed == 1)) {
		KERN_DEBUG("vmlaunch/vmresume failed: error %d.\n",
			   vmx->failed);
		return 1;
	} else if (unlikely(vmx->failed == 2)) {
		KERN_DEBUG("vmlaunch/vmresume failed: error %d, code 0x%08x.\n",
			   vmx->failed, vmcs_read32(VMCS_INSTRUCTION_ERROR));
		return 2;
	}

	vmx->g_rip = vmcs_read32(VMCS_GUEST_RIP);
	vmx->exit_reason = vmcs_read32(VMCS_EXIT_REASON);
	vmx->exit_qualification = vmcs_read32(VMCS_EXIT_QUALIFICATION);

	if (unlikely(vmx->exit_reason & EXIT_REASON_ENTRY_FAIL)) {
		KERN_DEBUG("VM-entry failure: reason %d.\n",
			   vmx->exit_reason & 0x0000ffff);
		return 1;
	}

	vmx->launched = 1;

	return vmx_handle_exit(vm);
}

static int
vmx_intercept_ioport(struct vm *vm, uint16_t port, bool enable)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx = (struct vmx *) vm->cookie;
	uint32_t *bitmap = (uint32_t *) vmx->io_bitmap;

	int entry = port / 32;
	int bit = port - entry * 32;

	if (enable == TRUE)
		bitmap[entry] |= (1 << bit);
	else
		bitmap[entry] &= ~(1 << bit);

	return 0;
}

static int
vmx_intercept_all_ioports(struct vm *vm, bool enable)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx = (struct vmx *) vm->cookie;
	uint32_t procbased_ctls;
	uint32_t *bitmap;

	procbased_ctls = vmcs_read32(VMCS_PRI_PROC_BASED_CTLS);
	if (enable == TRUE) {
		procbased_ctls |= PROCBASED_IO_EXITING;
	} else {
		procbased_ctls &= ~(PROCBASED_IO_EXITING | PROCBASED_IO_BITMAPS);
		bitmap = (uint32_t *) vmx->io_bitmap;
		memzero(bitmap, PAGESIZE * 2);
	}

	return 0;
}

static void
vmx_intercept_msr_helper(struct vmx *vmx, uint32_t msr, bool write, bool enable)
{
	uint32_t *msr_bitmap;
	uint32_t offset;
	int entry, bit;

	msr_bitmap = (uint32_t *)
		((uintptr_t) vmx->msr_bitmap + (write == TRUE) ? 0 : 2048);

	if (0x00000000 <= msr && msr <= 0x00001fff) {
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

static void
vmx_intercept_wrmsr(struct vmx *vmx, uint32_t msr, bool enable)
{

}

static int
vmx_intercept_msr(struct vm *vm, uint32_t msr, int rw)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx = (struct vmx *) vm->cookie;

	if (!((0x00000000 <= msr && msr <= 0x00001fff) ||
	      (0xc0000000 <= msr && msr <= 0xc0001fff)))
		return 1;

	vmx_intercept_msr_helper(vmx, msr, FALSE, (rw & 0x1) ? TRUE : FALSE);
	vmx_intercept_msr_helper(vmx, msr, TRUE, (rw & 0x2) ? TRUE : FALSE);

	return 0;
}

static int
vmx_intercept_all_msrs(struct vm *vm, int rw)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx = (struct vmx *) vm->cookie;
	char *msr_bitmap = vmx->msr_bitmap;
	char *rdmsr_bitmap = msr_bitmap;
	char *wrmsr_bitmap = (char *) ((uintptr_t) msr_bitmap + 0x800);

	memset(rdmsr_bitmap, (rw & 0x1) ? 0xf : 0x0, 2048);
	memset(wrmsr_bitmap, (rw & 0x2) ? 0xf : 0x0, 2048);

	return 0;
}

static int
vmx_intercept_intr_window(struct vm *vm, bool enable)
{
	KERN_ASSERT(vm != NULL);

	uint32_t procbased_ctls = vmcs_read32(VMCS_PRI_PROC_BASED_CTLS);
	if (enable == TRUE)
		procbased_ctls |= PROCBASED_INT_WINDOW_EXITING;
	else
		procbased_ctls &= ~PROCBASED_INT_WINDOW_EXITING;
	vmcs_write32(VMCS_PRI_PROC_BASED_CTLS, procbased_ctls);

	return 0;
}

static int
vmx_get_reg(struct vm *vm, guest_reg_t reg, uint32_t *val)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(val != NULL);

	struct vmx *vmx = (struct vmx *) vm->cookie;

	switch (reg) {
	case GUEST_EAX:
		*val = (uint32_t) vmx->g_rax;
		break;
	case GUEST_EBX:
		*val = (uint32_t) vmx->g_rbx;
		break;
	case GUEST_ECX:
		*val = (uint32_t) vmx->g_rcx;
		break;
	case GUEST_EDX:
		*val = (uint32_t) vmx->g_rdx;
		break;
	case GUEST_ESI:
		*val = (uint32_t) vmx->g_rsi;
		break;
	case GUEST_EDI:
		*val = (uint32_t) vmx->g_rdi;
		break;
	case GUEST_EBP:
		*val = (uint32_t) vmx->g_rbp;
		break;
	case GUEST_ESP:
		*val = vmcs_read32(VMCS_GUEST_RSP);
		break;
	case GUEST_EIP:
		*val = (uint32_t) vmx->g_rip;
		break;
	case GUEST_EFLAGS:
		*val = vmcs_read32(VMCS_GUEST_RFLAGS);
		break;
	case GUEST_CR0:
		*val = vmcs_read32(VMCS_GUEST_CR0);
		break;
	case GUEST_CR2:
		*val = (uint32_t) vmx->g_cr2;
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
vmx_set_reg(struct vm *vm, guest_reg_t reg, uint32_t val)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx = (struct vmx *) vm->cookie;

	switch (reg) {
	case GUEST_EAX:
		vmx->g_rax = val;
		break;
	case GUEST_EBX:
		vmx->g_rbx = val;
		break;
	case GUEST_ECX:
		vmx->g_rcx = val;
		break;
	case GUEST_EDX:
		vmx->g_rdx = val;
		break;
	case GUEST_ESI:
		vmx->g_rsi = val;
		break;
	case GUEST_EDI:
		vmx->g_rdi = val;
		break;
	case GUEST_EBP:
		vmx->g_rbp = val;
		break;
	case GUEST_ESP:
		vmcs_write32(VMCS_GUEST_RSP, val);
		break;
	case GUEST_EIP:
		vmx->g_rip = val;
		break;
	case GUEST_EFLAGS:
		vmcs_write32(VMCS_GUEST_RFLAGS, val);
		break;
	case GUEST_CR0:
		vmcs_write32(VMCS_GUEST_CR0, val);
		break;
	case GUEST_CR2:
		vmx->g_cr2 = val;
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
vmx_get_msr(struct vm *vm, uint32_t msr, uint64_t *val)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(val != NULL);

	if (!((0x00000000 <= msr && msr <= 0x00001fff) ||
	      (0xc0000000 <= msr && msr <= 0xc0001fff)))
		return 1;

	*val = rdmsr(msr);

	return 0;
}

static int
vmx_set_msr(struct vm *vm, uint32_t msr, uint64_t val)
{
	KERN_ASSERT(vm != NULL);

	if (!((0x00000000 <= msr && msr <= 0x00001fff) ||
	      (0xc0000000 <= msr && msr <= 0xc0001fff)))
		return 1;

	wrmsr(msr, val);

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
vmx_get_desc(struct vm *vm, guest_seg_t seg, struct guest_seg_desc *desc)
{
	KERN_ASSERT(vm != NULL);
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
vmx_set_desc(struct vm *vm, guest_seg_t seg, struct guest_seg_desc *desc)
{
	KERN_ASSERT(vm != NULL);
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
vmx_get_mmap(struct vm *vm, uintptr_t gpa, uintptr_t *hpa)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(hpa != NULL);

	struct vmx *vmx = (struct vmx *) vm->cookie;

	if (gpa % PAGESIZE)
	    return 1;

	*hpa = (uintptr_t) ept_gpa_to_hpa(vmx->pml4ept, gpa);
	return 0;
}

static int
vmx_set_mmap(struct vm *vm, uintptr_t gpa, uintptr_t hpa)
{
	KERN_ASSERT(vm != NULL);

	int rc = 0;
	struct vmx *vmx = (struct vmx *) vm->cookie;

	rc = ept_add_mapping(vmx->pml4ept, gpa, hpa, PAT_WRITE_BACK, FALSE);

	if (rc)
		return 1;
	else
		ept_invalidate_mappings((uint64_t)(uintptr_t) vmx->pml4ept);

	return 0;
}

static int
vmx_unset_mmap(struct vm *vm, uintptr_t gpa)
{
	KERN_PANIC("vmx_unset_mmap() not implemented yet.\n");
	return 1;
}

static int
vmx_inject_event(struct vm *vm, guest_event_t type,
		 uint8_t vector, uint32_t errcode, bool ev)
{
	KERN_ASSERT(vm != NULL);

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
vmx_get_next_eip(struct vm *vm, instr_t instr, uint32_t *val)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(val != NULL);
	struct vmx *vmx = (struct vmx *) vm->cookie;
	*val = vmx->g_rip + vmcs_read32(VMCS_EXIT_INSTRUCTION_LENGTH);
	return 0;
}

static int
vmx_get_cpuid(struct vm *vm, uint32_t in_eax, uint32_t in_ecx,
	      uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(eax != NULL && ebx != NULL && ecx != NULL && edx != NULL);

	if (in_eax == 0x00000001) {
		cpuid(in_eax, eax, ebx, ecx, edx);
		*ebx = /* 1 core per processor */
			(*ebx & ~(uint32_t) (0xf << 16)) | (1 << 16);
		*ecx = *ecx & ~(uint32_t)
			(CPUID_FEATURE_RDRAND | CPUID_FEATURE_AVX |
			 CPUID_FEATURE_AES | CPUID_FEATURE_TSC_DEADLINE |
			 CPUID_FEATURE_X2APIC | CPUID_FEATURE_PCID |
			 CPUID_FEATURE_XTPR | CPUID_FEATURE_EIST |
			 CPUID_FEATURE_SMX | CPUID_FEATURE_VMX |
			 CPUID_FEATURE_MONITOR);
		*edx = *edx & ~(uint32_t)
			(CPUID_FEATURE_HTT | CPUID_FEATURE_ACPI |
			 CPUID_FEATURE_MCA | CPUID_FEATURE_MTRR |
			 CPUID_FEATURE_APIC | CPUID_FEATURE_MCE |
			 CPUID_FEATURE_MSR | CPUID_FEATURE_DE);
	} else if (in_eax >= 0x80000000) {
		*eax = *ebx = *ecx = *edx = 0;
	} else {
		cpuid(in_eax, eax, ebx, ecx, edx);
	}

	return 0;
}

static bool
vmx_pending_event(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	return (vmcs_read32(VMCS_ENTRY_INTR_INFO) &
		VMCS_INTERRUPTION_INFO_VALID) ? TRUE : FALSE;
}

static bool
vmx_intr_shadow(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	return (vmcs_read32(VMCS_GUEST_INTERRUPTIBILITY) &
		(VMCS_INTERRUPTIBILITY_STI_BLOCKING |
		 VMCS_INTERRUPTIBILITY_MOVSS_BLOCKING)) ? TRUE : FALSE;
}

struct vmm_ops vmm_ops_intel = {
	.signature		= INTEL_VMX,
	.hw_init		= vmx_init,
	.intercept_ioport	= vmx_intercept_ioport,
	.intercept_all_ioports	= vmx_intercept_all_ioports,
	.intercept_msr		= vmx_intercept_msr,
	.intercept_all_msrs	= vmx_intercept_all_msrs,
	.intercept_intr_window	= vmx_intercept_intr_window,
	.vm_init		= vmx_init_vm,
	.vm_run			= vmx_run_vm,
	.get_reg		= vmx_get_reg,
	.set_reg		= vmx_set_reg,
	.get_msr		= vmx_get_msr,
	.set_msr		= vmx_set_msr,
	.get_desc		= vmx_get_desc,
	.set_desc		= vmx_set_desc,
	.get_mmap		= vmx_get_mmap,
	.set_mmap		= vmx_set_mmap,
	.unset_mmap		= vmx_unset_mmap,
	.inject_event		= vmx_inject_event,
	.get_next_eip		= vmx_get_next_eip,
	.get_cpuid		= vmx_get_cpuid,
	.pending_event		= vmx_pending_event,
	.intr_shadow		= vmx_intr_shadow,
};
