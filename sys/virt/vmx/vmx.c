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
	struct vmx	vmxs[MAX_CPU];
	bool		used[MAX_CPU];
} vmx_pool;

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
} vmx_proc_info[MAX_CPU];

#if defined (DEBUG_VMEXIT) || defined (TRACE_VMEXIT)

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

#ifdef TRACE_VMEXIT

static struct {
	uint64_t total;
	uint64_t delta;
	uint64_t time;
} vmexit_trace_info[60];

static void
dump_vmexit_trace_info(void)
{
	int slot;

	KERN_DEBUG("VMEXIT trace:\n");

	for (slot = 0; slot < 60; slot++) {
		if (vmexit_trace_info[slot].delta > 0) {
			vmexit_trace_info[slot].total +=
				vmexit_trace_info[slot].delta;
			dprintf("%s: +%lld times, "
				"total %lld ticks, "
				"%lld ticks/time\n",
				exit_reason_string[slot],
				vmexit_trace_info[slot].delta,
				vmexit_trace_info[slot].time,
				vmexit_trace_info[slot].time/
				vmexit_trace_info[slot].total);
			vmexit_trace_info[slot].delta = 0;
		}
	}
}

#endif

#ifdef TRACE_EVT_INJECT
static uint64_t exception_delta = 0;
static uint64_t intr_delta = 0;
static uint64_t others_delta = 0;

static void
dump_evt_inj_trace_info(void)
{
	dprintf("Inject");
	if (exception_delta) {
		dprintf(" +%d exceptions", exception_delta);
		exception_delta = 0;
	}
	if (intr_delta) {
		dprintf(" +%d interrupts", intr_delta);
		intr_delta = 0;
	}
	if (others_delta) {
		dprintf(" +%d others", others_delta);
		others_delta = 0;
	}
	dprintf("\n");
}

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

static void
vmx_set_intercept_io(struct vmx *vmx, uint16_t port, data_sz_t sz, bool enable)
{
	KERN_ASSERT(vmx != NULL);
	KERN_ASSERT(sz == SZ8 || sz == SZ16 || sz == SZ32);

#ifdef DEBUG_GUEST_IOIO
	if (enable == TRUE)
		KERN_DEBUG("Enable intercepting I/O port 0x%x, size %d byte%s\n",
			   port, (1 << sz), (sz != SZ8) ? "s" : "");
#endif

	uint32_t *bitmap = (uint32_t *) vmx->io_bitmap;

	int i;
	int port1, entry, bit;

	for (i = 0; i < (1 << sz); i++) {
		port1 = port + i;
		entry = port1 / 32;
		bit = port1 - entry *32;

		if (enable == TRUE)
			bitmap[entry] |= (1 << bit);
		else
			bitmap[entry] &= ~(1 << bit);
	}
}

static void
vmx_inject_event(uint32_t type, uint8_t vector, uint32_t err_code, int err_valid)
{
	KERN_ASSERT(type == VMCS_INTERRUPTION_INFO_HW_INTR ||
		    type == VMCS_INTERRUPTION_INFO_NMI ||
		    type == VMCS_INTERRUPTION_INFO_HW_EXCPT);

#ifdef DEBUG_EVT_INJECT
	KERN_DEBUG("Inject %s, vector 0x%x, error code %d.\n",
		   (type == VMCS_INTERRUPTION_INFO_HW_INTR) ? "ExtINT" :
		   (type == VMCS_INTERRUPTION_INFO_NMI) ? "NMI" : "HW Excpt",
		   vector, err_code);
#endif

	uint32_t intr_info;

	intr_info = vmcs_read32(VMCS_ENTRY_INTR_INFO);
	if (intr_info & VMCS_INTERRUPTION_INFO_VALID)
		KERN_PANIC("Non-delivered event: type %d, vector %d.\n",
			   (intr_info >> 8) & 7, intr_info & 0xff);

	intr_info = VMCS_INTERRUPTION_INFO_VALID |
		type | vector | ((err_valid ? 1 : 0) << 11);

#ifdef DEBUG_EVT_INJECT
	KERN_DEBUG("VMCS_ENTRY_INTR_INFO 0x%08x\n", intr_info);
	KERN_DEBUG("VMCS_GUEST_INTERRUPTIBILITY 0x%08x\n",
		   (uintptr_t) vmcs_read32(VMCS_GUEST_INTERRUPTIBILITY));
	KERN_DEBUG("guest eflags 0x%08x\n",
		   (uintptr_t) vmcs_read32(VMCS_GUEST_RFLAGS));
#endif

#ifdef TRACE_EVT_INJECT
	if (type == VMCS_INTERRUPTION_INFO_HW_INTR)
		intr_delta++;
	else if (type == VMCS_INTERRUPTION_INFO_HW_EXCPT)
		exception_delta++;
	else
		others_delta++;
#endif

	vmcs_write32(VMCS_ENTRY_INTR_INFO, intr_info);

	if (err_valid) {
#ifdef DEBUG_EVT_INJECT
		KERN_DEBUG("VMCS_ENTRY_EXCEPTION_ERROR 0x%08x\n", err_code);
#endif
		vmcs_write32(VMCS_ENTRY_EXCEPTION_ERROR, err_code);
	}
}

static void
load_bios(uint64_t *pml4ept)
{
	/* load BIOS ROM */
	extern uint8_t _binary___misc_bios_bin_start[],
		_binary___misc_bios_bin_size[];

	KERN_ASSERT((size_t) _binary___misc_bios_bin_size % 0x10000 == 0);

	uintptr_t bios_addr = 0x100000 - (size_t) _binary___misc_bios_bin_size;

	ept_copy_to_guest(pml4ept,
			  bios_addr,
			  (uintptr_t) _binary___misc_bios_bin_start,
			  (size_t) _binary___misc_bios_bin_size);

	/* load VGA BIOS ROM */
	extern uint8_t _binary___misc_vgabios_bin_start[],
		_binary___misc_vgabios_bin_size[];

	/* KERN_ASSERT((size_t) _binary___misc_vgabios_bin_size <= 0x8000); */

	ept_copy_to_guest(pml4ept,
			  0xc0000,
			  (uintptr_t) _binary___misc_vgabios_bin_start,
			  (size_t) _binary___misc_vgabios_bin_size);
}

static struct vmx *
vmx_alloc(void)
{
	int i;

	for (i = 0; i < MAX_CPU; i++)
		if (vmx_pool.used[i] == FALSE)
			break;

	if (i == MAX_CPU)
		return NULL;

	vmx_pool.used[i] = TRUE;
	memset(&vmx_pool.vmxs[i], 0, sizeof(struct vmx));

	return &vmx_pool.vmxs[i];
}

static int
vmx_enable(void)
{
	pageinfo_t *pi;
	struct vmx_info *vmx_info;
	int error;

	vmx_info = &vmx_proc_info[pcpu_cpu_idx(pcpu_cur())];

	lcr4(rcr4() | CR4_VMXE);

	if ((pi = mem_page_alloc()) == NULL) {
		KERN_DEBUG("Cannot allocate memory for VMX regison.\n");
		return 1;
	}
	vmx_info->vmx_region = (void *) mem_pi2phys(pi);

	*(uint32_t *) vmx_info->vmx_region = rdmsr(MSR_VMX_BASIC) & 0xffffffff;

	error = vmxon(vmx_info->vmx_region);
	if (!error)
		vmx_info->vmx_enabled = TRUE;
	else
		vmx_info->vmx_enabled = FALSE;

	return error;
}

static int
vmx_init(void)
{
	int error;
	uint32_t dummy, val;
	uint64_t fixed0, fixed1;
	struct vmx_info *vmx_info;

	KERN_ASSERT(pcpu_cur() != NULL);
	vmx_info = &vmx_proc_info[pcpu_cpu_idx(pcpu_cur())];

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
	pageinfo_t *vmcs_pi, *ept_pi, *msr_pi, *io_pi;

	vmx_info = &vmx_proc_info[pcpu_cpu_idx(pcpu_cur())];
	KERN_ASSERT(vmx_info->vmx_enabled == TRUE);

	vmx = vmx_alloc();

	if (vmx == NULL) {
		KERN_DEBUG("Cannot allocate memory for struct vmx.\n");
		return 1;
	}

	vm->cookie = vmx;

	/*
	 * Setup VMCS.
	 */
	if ((vmcs_pi = mem_page_alloc()) == NULL) {
		KERN_DEBUG("Cannot allocate memory for VMCS.\n");
		return 1;
	}
	vmx->vmcs = (struct vmcs *) mem_pi2phys(vmcs_pi);
	KERN_DEBUG("VMCS @ 0x%08x\n", vmx->vmcs);

	/*
	 * Setup EPT.
	 */
	if ((ept_pi = mem_page_alloc()) == NULL) {
		KERN_DEBUG("Cannot allocate memory for EPT.\n");
		mem_page_free(vmcs_pi);
		return 1;
	}
	vmx->pml4ept = (uint64_t *) mem_pi2phys(ept_pi);
	memzero(mem_pi2ptr(ept_pi), PAGESIZE);
	KERN_DEBUG("EPT @ 0x%08x.\n", vmx->pml4ept);

	if (ept_create_mappings(vmx->pml4ept, VM_PHY_MEMORY_SIZE)) {
		KERN_DEBUG("Cannot create EPT mappings.\n");
		mem_page_free(ept_pi);
		mem_page_free(vmcs_pi);
		return 1;
	}

	/* load SeaBIOS */
	load_bios(vmx->pml4ept);

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
	if ((msr_pi = mem_page_alloc()) == NULL) {
		KERN_DEBUG("Cannot allocate memory for MSR bitmap.\n");
		mem_page_free(vmcs_pi);
		mem_page_free(ept_pi);
		return 1;
	}
	vmx->msr_bitmap = (char *) mem_pi2phys(msr_pi);
	KERN_DEBUG("MSR bitmap @ 0x%08x\n", vmx->msr_bitmap);
	msr_bitmap_initialize(vmx->msr_bitmap);

	/*
	 * Setup I/O bitmaps.
	 */
	if ((io_pi = mem_pages_alloc(PAGESIZE * 2)) == NULL) {
		KERN_DEBUG("Cannot allocate memory for I/O bitmap.\n");
		mem_page_free(vmcs_pi);
		mem_page_free(ept_pi);
		return 1;
	}
	vmx->io_bitmap = (char *) mem_pi2phys(io_pi);
	memset(vmx->io_bitmap, 0x0, PAGESIZE * 2);
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

	vmx->g_rax = vmx->g_rbx = vmx->g_rcx =
		vmx->g_rsi = vmx->g_rdi = vmx->g_rbp = 0;
	vmx->g_rdx =
		(pcpu_cur()->arch_info.family << 8) |
		(pcpu_cur()->arch_info.model << 4) |
		(pcpu_cur()->arch_info.step);
	vmx->g_cr2 = 0;
	vmx->g_dr0 = vmx->g_dr1 = vmx->g_dr2 = vmx->g_dr3 = 0;
	vmx->g_dr6 = 0xffff0ff0;

	KERN_DEBUG("vmx_init_vm() done.\n");

	return 0;
}

static int
vmx_run_vm(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx = (struct vmx *) vm->cookie;

	vmptrld(vmx->vmcs);

	vmcs_write32(VMCS_GUEST_RIP, vmx->g_rip);

#ifdef DEBUG_VMENTRY
	if (vmx->launched)
		KERN_DEBUG("Resume VM from eip 0x%08x:0x%08x ... \n",
			   (uintptr_t) vmcs_read32(VMCS_GUEST_CS_BASE),
			   (uintptr_t) vmcs_read32(VMCS_GUEST_RIP));
	else
		KERN_DEBUG("Launch VM from eip 0x%08x:0x%08x ... \n",
			   (uintptr_t) vmcs_read32(VMCS_GUEST_CS_BASE),
			   (uintptr_t) vmcs_read32(VMCS_GUEST_RIP));
	/* vmx_dump_info(vmx); */
#endif

#ifdef TRACE_TOTAL_TIME
	if (vm->start_tsc != 0)
		vm->total_tsc += (rdtscp() - vm->start_tsc);
#endif

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

#ifdef TRACE_TOTAL_TIME
	vm->start_tsc = rdtscp();
#endif

	if (unlikely(vmx->failed == 1))
		KERN_PANIC("vmlaunch/vmresume failed: error %d.\n",
			   vmx->failed);
	else if (unlikely(vmx->failed == 2))
		KERN_PANIC("vmlaunch/vmresume failed: error %d, code 0x%08x.\n",
			   vmx->failed, vmcs_read32(VMCS_INSTRUCTION_ERROR));

	vmx->g_rip = vmcs_read32(VMCS_GUEST_RIP);
	vmx->exit_reason = vmcs_read32(VMCS_EXIT_REASON);
	vmx->exit_qualification = vmcs_read32(VMCS_EXIT_QUALIFICATION);

	if (unlikely(vmx->exit_reason & EXIT_REASON_ENTRY_FAIL)) {
		KERN_PANIC("VM-entry failure: reason %d.\n",
			   vmx->exit_reason & 0x0000ffff);
		return 1;
	}

	if ((vmx->exit_reason & EXIT_REASON_MASK) == EXIT_REASON_EXT_INTR) {
		vm->exit_reason = EXIT_FOR_EXTINT;
		vm->handled = FALSE;
	}

	vmx->launched = 1;

	return 0;
}

static gcc_inline uint32_t
vmx_interruption_info(void)
{
	return vmcs_read32(VMCS_EXIT_INTERRUPTION_INFO);
}

static gcc_inline uint32_t
vmx_vector_info(void)
{
	return vmcs_read32(VMCS_IDT_VECTORING_INFO);
}

static void
physical_ioport_read(uint32_t port, void *data, data_sz_t size)
{
	KERN_ASSERT(data != NULL);

	switch (size) {
	case SZ8:
		*(uint8_t *) data = inb(port);
		break;

	case SZ16:
		*(uint16_t *) data = inw(port);
		break;

	case SZ32:
		*(uint32_t *) data = inl(port);
		break;

	default:
		KERN_PANIC("Invalid data size.\n");
	}
}

static void
physical_ioport_write(uint32_t port, void *data, data_sz_t size)
{
	KERN_ASSERT(data != NULL);

	switch (size) {
	case SZ8:
		outb(port, *(uint8_t *) data);
		break;

	case SZ16:
		outw(port, *(uint16_t *) data);
		break;

	case SZ32:
		outl(port, *(uint32_t *) data);
		break;

	default:
		KERN_PANIC("Invalid data size.\n");
	}
}

static gcc_inline bool
vmx_handle_inout(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx = (struct vmx *) vm->cookie;

	uint16_t port;
	uint8_t size, dir;
	data_sz_t width;
	vid_t vid;

	port = EXIT_QUAL_IO_PORT(vmx->exit_qualification);
	size = EXIT_QUAL_IO_SIZE(vmx->exit_qualification);
	dir = EXIT_QUAL_IO_DIR(vmx->exit_qualification);
	vid = vm->vdev.ioport[port].vid;

	KERN_ASSERT(size == EXIT_QUAL_IO_ONE_BYTE ||
		    size == EXIT_QUAL_IO_TWO_BYTE ||
		    size == EXIT_QUAL_IO_FOUR_BYTE);

	if (size == EXIT_QUAL_IO_ONE_BYTE)
		width = SZ8;
	else if (size == EXIT_QUAL_IO_TWO_BYTE)
		width = SZ16;
	else
		width = SZ32;

#ifdef DEBUG_GUEST_IOIO
	KERN_DEBUG("Intercept IO: %s, port 0x%x, %d bytes%s%s.\n",
		   (dir == EXIT_QUAL_IO_OUT) ? "OUT" : "IN",
		   port, (1 << width),
		   EXIT_QUAL_IO_REP(vmx->exit_qualification) ? ", REP" : "",
		   EXIT_QUAL_IO_STR(vmx->exit_qualification) ? ", STR" : "");
#endif

	if (dir == EXIT_QUAL_IO_IN)
		vdev_read_guest_ioport(vm, vid,
				       port, width, (uint32_t *) &vmx->g_rax);
	else
		vdev_write_guest_ioport(vm, vid, port, width, (uint32_t) vmx->g_rax);

	vmx->g_rip += vmcs_read32(VMCS_EXIT_INSTRUCTION_LENGTH);

	return TRUE;
}

static gcc_inline bool
vmx_handle_cpuid(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx = (struct vmx *) vm->cookie;
	uint32_t id;
	uint32_t eax, ebx, ecx, edx;

	id = vmx->g_rax;

#ifdef DEBUG_GUEST_CPUID
	KERN_DEBUG("Intercept cpuid 0x%08x.\n", (uintptr_t) id);
#endif

	if (id == 0x00000001) {
		cpuid(id, &eax, &ebx, &ecx, &edx);
		vmx->g_rax = eax;
		vmx->g_rbx = /* 1 core per processor */
			(ebx & ~(uint32_t) (0xf << 16)) | (1 << 16);
		vmx->g_rcx = ecx & ~(uint32_t) (CPUID_FEATURE_RDRAND |
						CPUID_FEATURE_AVX |
						CPUID_FEATURE_AES |
						CPUID_FEATURE_TSC_DEADLINE |
						CPUID_FEATURE_X2APIC |
						CPUID_FEATURE_PCID |
						CPUID_FEATURE_XTPR |
						CPUID_FEATURE_EIST |
						CPUID_FEATURE_SMX |
						CPUID_FEATURE_VMX |
						CPUID_FEATURE_MONITOR);
		vmx->g_rdx = edx & ~(uint32_t) (CPUID_FEATURE_HTT |
						CPUID_FEATURE_ACPI |
						CPUID_FEATURE_MCA |
						CPUID_FEATURE_MTRR |
						CPUID_FEATURE_APIC |
						CPUID_FEATURE_MCE |
						CPUID_FEATURE_MSR |
						CPUID_FEATURE_DE);
	} else if (id >= 0x80000000) {
		vmx->g_rax = vmx->g_rbx = vmx->g_rcx = vmx->g_rdx = 0;
	} else {
		cpuid(id, &eax, &ebx, &ecx, &edx);
		vmx->g_rax = eax;
		vmx->g_rbx = ebx;
		vmx->g_rcx = ecx;
		vmx->g_rdx = edx;
	}

	vmx->g_rip += vmcs_read32(VMCS_EXIT_INSTRUCTION_LENGTH);

	return TRUE;
}

static gcc_inline bool
vmx_handle_rdmsr(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx;
	uint32_t msr;
	uint64_t val;
	uint16_t cs_sel;
	uint8_t cpl;
	uint32_t err_code;

	vmx = (struct vmx *) vm->cookie;
	msr = vmx->g_rcx;
	cpl = (vmcs_read32(VMCS_GUEST_CS_ACCESS_RIGHTS) >> 5) & 3;

#ifdef DEBUG_GUEST_MSR
	KERN_DEBUG("Intercept rdmsr: MSR 0x%08x, CPL %d.\n", msr, cpl);
#endif

	if (cpl != 0 &&
	    !((0x00000000 <= msr && msr <= 0x00001fff) ||
	      (0xc0000000 <= msr && msr <= 0xc0001fff))) {
		cs_sel = vmcs_read16(VMCS_GUEST_CS_SELECTOR);
		err_code = (cs_sel & 0x1fff) << 3;
		vmx_inject_event(VMCS_INTERRUPTION_INFO_HW_EXCPT, T_GPFLT,
				 err_code, 1);
		return TRUE;
	}

	val = rdmsr(msr);
	vmx->g_rax = val & 0xffffffff;
	vmx->g_rdx = (val >> 32) & 0xffffffff;
	vmx->g_rip += vmcs_read32(VMCS_EXIT_INSTRUCTION_LENGTH);

	return TRUE;
}

static gcc_inline bool
vmx_handle_wrmsr(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx;
	uint32_t msr;
	uint64_t val;
	uint16_t cs_sel;
	uint8_t cpl;
	uint32_t err_code;

	vmx = (struct vmx *) vm->cookie;
	msr = vmx->g_rcx;
	val = ((vmx->g_rdx & 0xffffffff) << 32) | (vmx->g_rax & 0xffffffff);
	cpl = (vmcs_read32(VMCS_GUEST_CS_ACCESS_RIGHTS) >> 5) & 3;

#ifdef DEBUG_GUEST_MSR
	KERN_DEBUG("Intecept wrmsr: MSR 0x%08x, val 0x%llx, CPL %d.\n",
		   msr, val, cpl);
#endif

	if (cpl != 0 &&
	    !((0x00000000 <= msr && msr <= 0x00001fff) ||
	      (0xc0000000 <= msr && msr <= 0xc0001fff))) {
		cs_sel = vmcs_read16(VMCS_GUEST_CS_SELECTOR);
		err_code = (cs_sel & 0x1fff) << 3;
		vmx_inject_event(VMCS_INTERRUPTION_INFO_HW_EXCPT, T_GPFLT,
				 err_code, 1);
		return TRUE;
	}

	wrmsr(msr, val);
	vmx->g_rip += vmcs_read32(VMCS_EXIT_INSTRUCTION_LENGTH);

	return TRUE;
}

static gcc_inline bool
vmx_handle_ept_violation(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx;
	uintptr_t fault_pa;
	pageinfo_t *pi;

	vmx = (struct vmx *) vm->cookie;
	fault_pa = vmcs_read64(VMCS_GUEST_PHYSICAL_ADDRESS);

#ifdef DEBUG_EPT
	KERN_DEBUG("Intercept EPT violation: "
		   "guest physical address 0x%08x, "
		   "guest linear address 0x%08x\n",
		   fault_pa,
		   (uintptr_t) vmcs_read32(VMCS_GUEST_LINEAR_ADDRESS));
#endif

	if (!(0xf0000000 <= fault_pa && fault_pa <= 0xffffffff)) {
		KERN_WARN("Out of physical memory range: 0x%08x.\n", fault_pa);
		return FALSE;
	}

	if ((pi = mem_page_alloc()) == NULL) {
		KERN_WARN("Cannot allocate physical memory for gpa 0x%08x.\n",
			  fault_pa);
		return FALSE;
	}

	memset(mem_pi2ptr(pi), 0, PAGESIZE);

	if (ept_add_mapping(vmx->pml4ept,
			    ROUNDDOWN(fault_pa, PAGESIZE),
			    mem_pi2phys(pi), PAT_WRITE_BACK, FALSE))
		KERN_PANIC("Cannot allocate memory for guest physical "
			   "address 0x%08x.\n", fault_pa);

	ept_invalidate_mappings((uintptr_t) vmx->pml4ept);

	return TRUE;
}

static void
vmx_intr_assist(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx = (struct vmx *) vm->cookie;
	int vector, intr_disable, blocked, pending;
	uint32_t procbased_ctls;

	if ((vector = vdev_peep_intout(vm)) == -1)
		return;

	pending = vmcs_read32(VMCS_EXIT_INTERRUPTION_INFO) &
		VMCS_INTERRUPTION_INFO_VALID;

#ifdef DEBUG_GUEST_INTR
	if (pending)
		KERN_DEBUG("Find pending event: intr_info 0x%08x.\n",
			   vmcs_read32(VMCS_EXIT_INTERRUPTION_INFO));
#endif

	if (((uint32_t) vmcs_read32(VMCS_GUEST_RFLAGS) & FL_IF) == 0)
		intr_disable = 1;
	else
	        intr_disable = 0;

#ifdef DEBUG_GUEST_INTR
	if (intr_disable)
		KERN_DEBUG("Guest EFLAGS.IF is cleared: 0x%08x\n",
			   (uintptr_t) vmcs_read32(VMCS_GUEST_RFLAGS));
#endif

	blocked = vmcs_read32(VMCS_GUEST_INTERRUPTIBILITY) &
		(VMCS_INTERRUPTIBILITY_STI_BLOCKING |
		 VMCS_INTERRUPTIBILITY_MOVSS_BLOCKING);

#ifdef DEBUG_GUEST_INTR
	if (blocked & VMCS_INTERRUPTIBILITY_STI_BLOCKING)
		KERN_DEBUG("Blocked by STI.\n");
	if (blocked & VMCS_INTERRUPTIBILITY_MOVSS_BLOCKING)
		KERN_DEBUG("Blocked by MOVSS.\n");
#endif

	blocked = blocked || intr_disable;

	if (pending || blocked) {
		procbased_ctls = vmcs_read32(VMCS_PRI_PROC_BASED_CTLS);
		vmcs_write32(VMCS_PRI_PROC_BASED_CTLS,
			     procbased_ctls | PROCBASED_INT_WINDOW_EXITING);
		vmx->pending_intr = vector;
		return;
	}

	if ((vector = vdev_read_intout(vm)) == -1)
		return;

	vmx_inject_event(VMCS_INTERRUPTION_INFO_HW_INTR, vector, 0, 0);
	vmx->pending_intr = -1;

	procbased_ctls = vmcs_read32(VMCS_PRI_PROC_BASED_CTLS);
	vmcs_write32(VMCS_PRI_PROC_BASED_CTLS,
		     procbased_ctls & ~PROCBASED_INT_WINDOW_EXITING);
}

static int
vmx_handle_exit(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx;
	uint32_t procbased_ctls;

#if defined (TRACE_VMEXIT) || defined (TRACE_EVT_INJECT)
	uint64_t start_tsc, end_tsc;
	static uint64_t last_dump_tsc = 0;
	extern uint64_t tsc_per_ms;
#endif

	vmx = (struct vmx *) vm->cookie;
	KERN_ASSERT(vmx != NULL);

	if ((vmx->exit_reason & EXIT_REASON_ENTRY_FAIL)) {
		KERN_DEBUG("VMEXIT for VM-entry failure.\n");
		vm->exit_reason = EXIT_FOR_OTHERS;
		vm->handled = 0;
		goto ret;
	}

#ifdef DEBUG_VMEXIT
	KERN_DEBUG("Exit reason %s, qualification 0x%llx, eip 0x%08x\n",
		   exit_reason_string[vmx->exit_reason & EXIT_REASON_MASK],
		   vmx->exit_qualification,
		   (uintptr_t) vmcs_read32(VMCS_GUEST_RIP));
#endif

#ifdef TRACE_VMEXIT
	vmexit_trace_info[vmx->exit_reason & EXIT_REASON_MASK].delta++;
	start_tsc = rdtscp();
#endif

	switch (vmx->exit_reason & EXIT_REASON_MASK) {
	case EXIT_REASON_EXT_INTR:
		/* external interrupts should be handled before here */
		KERN_ASSERT(vm->exit_reason == EXIT_FOR_EXTINT &&
			    vm->handled == TRUE);
		break;

	case EXIT_REASON_INOUT:
		vm->exit_reason = EXIT_FOR_OTHERS;
		vm->handled = vmx_handle_inout(vm);
		break;

	case EXIT_REASON_CPUID:
		vm->exit_reason = EXIT_FOR_OTHERS;
		vm->handled = vmx_handle_cpuid(vm);
		break;

	case EXIT_REASON_RDMSR:
		vm->exit_reason = EXIT_FOR_OTHERS;
		vm->handled = vmx_handle_rdmsr(vm);
		break;

	case EXIT_REASON_WRMSR:
		vm->exit_reason = EXIT_FOR_OTHERS;
		vm->handled = vmx_handle_wrmsr(vm);
		break;

	case EXIT_REASON_RDTSC:
		vm->exit_reason = EXIT_FOR_OTHERS;
		vmx->g_rdx = (vm->tsc >> 32);
		vmx->g_rax = (vm->tsc &0xffffffff);
		vmx->g_rip += vmcs_read32(VMCS_EXIT_INSTRUCTION_LENGTH);
		vm->handled = TRUE;
		break;

	case EXIT_REASON_RDTSCP:
	case EXIT_REASON_HLT:
	case EXIT_REASON_VMCALL:
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
		vm->exit_reason = EXIT_FOR_OTHERS;
		KERN_DEBUG("Invalid OP: exit_reason 0x%08x.\n",
			   ((struct vmx *) vm->cookie)->exit_reason);
		vmx_inject_event(VMCS_INTERRUPTION_INFO_HW_EXCPT, T_ILLOP, 0, 0);
		vm->handled = TRUE;
		break;

	case EXIT_REASON_EPT_FAULT:
		vm->exit_reason = EXIT_FOR_OTHERS;
		vm->handled = vmx_handle_ept_violation(vm);
		break;

	case EXIT_REASON_INTR_WINDOW:
		/* disable exiting for the interrupt window */
		vm->exit_reason = EXIT_FOR_OTHERS;
		procbased_ctls = vmcs_read32(VMCS_PRI_PROC_BASED_CTLS);
		procbased_ctls &= ~PROCBASED_INT_WINDOW_EXITING;
		vmcs_write32(VMCS_PRI_PROC_BASED_CTLS, procbased_ctls);
		vm->handled = TRUE;
		break;

	case EXIT_REASON_EXCEPTION:
	case EXIT_REASON_TRIPLE_FAULT:
	case EXIT_REASON_NMI_WINDOW:
	case EXIT_REASON_INVAL_VMCS:
	case EXIT_REASON_INVAL_MSR:
	case EXIT_REASON_MCE:
	case EXIT_REASON_EPT_MISCONFIG:
	default:
		KERN_DEBUG("Unhandled VMEXIT: exit_reason 0x%08x.\n",
			   vmx->exit_reason);
		vm->exit_reason = EXIT_FOR_OTHERS;
		vm->handled = FALSE;
	}

#if defined (TRACE_VMEXIT) || defined (TRACE_EVT_INJECT)
	end_tsc = rdtscp();

#ifdef TRACE_VMEXIT
	vmexit_trace_info[vmx->exit_reason & EXIT_REASON_MASK].time +=
		(end_tsc - start_tsc);
#endif

	if ((end_tsc - last_dump_tsc) / tsc_per_ms > 10 * 1000) {
		last_dump_tsc = rdtscp();
#ifdef TRACE_VMEXIT
		dump_vmexit_trace_info();
#endif
#ifdef TRACE_EVT_INJECT
		dump_evt_inj_trace_info();
#endif
	}
#endif

	KERN_ASSERT(vm->handled == TRUE);

	vmx_intr_assist(vm);

 ret:
	return vm->handled;
}

static int
vmx_handle_extint(struct vm *vm, uint8_t irq)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(vm->exit_reason == EXIT_FOR_EXTINT &&
		    vm->handled == FALSE);

	vid_t vid = vm->vdev.irq[irq].vid;

	/*
	 * Try to notify the virtual device which occupies the interrupt. If no
	 * virtual device is registered as the source of the interrupt, raise
	 * corresponding interrupt line of the virtual PIC.
	 */
	if (vdev_sync_dev(vm, vid) && vpic_is_ready(&vm->vdev.vpic) == TRUE) {
		vdev_raise_irq(vm, vid, irq);
		vdev_lower_irq(vm, vid, irq);
	}

	vm->handled = TRUE;

	return 0;
}

static void
vmx_intercept_io(struct vm *vm, uint32_t port, data_sz_t size, bool enable)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx = (struct vmx *) vm->cookie;
	vmx_set_intercept_io(vmx, port, size, enable);
}

static uint64_t
vmx_get_enter_tsc(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	struct vmx *vmx = (struct vmx *) vm->cookie;
	return ((uint64_t) vmx->enter_tsc[1] << 32) | vmx->enter_tsc[0];
}

static uint64_t
vmx_get_exit_tsc(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	struct vmx *vmx = (struct vmx *) vm->cookie;
	return ((uint64_t) vmx->exit_tsc[1] << 32) | vmx->exit_tsc[0];
}

static uintptr_t
vmx_gpa_2_hpa(struct vm *vm, uintptr_t gpa)
{
	KERN_ASSERT(vm != NULL);

	struct vmx *vmx = (struct vmx *) vm->cookie;

	return ept_gpa_to_hpa(vmx->pml4ept, gpa);
}

struct vmm_ops vmm_ops_intel = {
	.signature		= INTEL_VMX,
	.vmm_init		= vmx_init,
	.vm_init		= vmx_init_vm,
	.vm_run			= vmx_run_vm,
	.vm_exit_handle		= vmx_handle_exit,
	.vm_intr_handle		= vmx_handle_extint,
	.vm_enter_tsc		= vmx_get_enter_tsc,
	.vm_exit_tsc		= vmx_get_exit_tsc,
	.vm_intercept_ioio	= vmx_intercept_io,
	.vm_translate_gp2hp	= vmx_gpa_2_hpa,
};
