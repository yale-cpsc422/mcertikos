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
#include <preinit/lib/x86.h>
#include <preinit/dev/pic.h>
#include <preinit/dev/vmx_drv.h>

#include <lib/x86.h>
#include <sys/dev/tsc.h>

#include <kern/virt/vmm.h>

#include "ept.h"
#include "vmcs.h"
#include "vmx.h"
#include "vmx_controls.h"
#include "x86.h"

extern void *memset(void *v, unsigned int c, unsigned int n);

struct vmx vmx gcc_aligned(PAGESIZE);

struct vmx_info vmx_info;
char msr_bitmap[PAGESIZE];
char io_bitmap[PAGESIZE * 2];

static gcc_inline uint32_t
read_esp(void)
{
    uint32_t esp;
    __asm __volatile("movl %%esp,%0" : "=rm" (esp));
    return esp;
}

static gcc_inline uint32_t
read_eax(void)
{
    uint32_t val;
    __asm __volatile("movl %%eax,%0" : "=rm" (val));
    return val;
}

static gcc_inline uint32_t
read_ebx(void)
{
    uint32_t val;
    __asm __volatile("movl %%ebx,%0" : "=rm" (val));
    return val;
}

static gcc_inline uint32_t
read_ecx(void)
{
    uint32_t val;
    __asm __volatile("movl %%ecx,%0" : "=rm" (val));
    return val;
}

static gcc_inline uint32_t
read_edx(void)
{
    uint32_t val;
    __asm __volatile("movl %%edx,%0" : "=rm" (val));
    return val;
}

static gcc_inline uint32_t
read_esi(void)
{
    uint32_t val;
    __asm __volatile("movl %%esi,%0" : "=rm" (val));
    return val;
}

static gcc_inline uint32_t
read_edi(void)
{
    uint32_t val;
    __asm __volatile("movl %%edi,%0" : "=rm" (val));
    return val;
}

static gcc_inline uint32_t
read_eflags(void)
{
        uint32_t flags;

        __asm __volatile("pushfl; popl %0" : "=r" (flags));
        return flags;
}

static void
vmx_dump_info()
{
#ifdef DEBUG_VMX
    uintptr_t vmcs_ptr;

    vmptrst(&vmcs_ptr);
    VMX_DEBUG("VMCS @ 0x%08x\n", vmcs_ptr);
    KERN_ASSERT(vmcs_ptr == (uintptr_t) vmx.vmcs);
    vmptrld(vmx.vmcs);

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
        (uintptr_t) vmx.g_rax, (uintptr_t) vmx.g_rbx,
        (uintptr_t) vmx.g_rcx, (uintptr_t) vmx.g_rdx,
        (uintptr_t) vmx.g_rsi, (uintptr_t) vmx.g_rdi,
        (uintptr_t) vmx.g_rbp, (uintptr_t) vmcs_read32(VMCS_GUEST_RSP),
        (uintptr_t) vmcs_read32(VMCS_GUEST_RIP),
        (uintptr_t) vmcs_read32(VMCS_GUEST_RFLAGS),
        (uintptr_t) vmcs_read32(VMCS_GUEST_CR0),
        (uintptr_t) vmx.g_cr2,
        (uintptr_t) vmcs_read32(VMCS_GUEST_CR3),
        (uintptr_t) vmcs_read32(VMCS_GUEST_CR4),
        (uintptr_t) vmx.g_dr0, (uintptr_t) vmx.g_dr1,
        (uintptr_t) vmx.g_dr2, (uintptr_t) vmx.g_dr2,
        (uintptr_t) vmx.g_dr6, (uintptr_t) vmcs_read32(VMCS_GUEST_DR7),
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
#endif
}

void
vmx_set_intercept_intwin(unsigned int enable)
{
	unsigned int procbased_ctls = vmcs_read32(VMCS_PRI_PROC_BASED_CTLS);
	if (enable == 1)
		procbased_ctls |= PROCBASED_INT_WINDOW_EXITING;
	else
		procbased_ctls &= ~PROCBASED_INT_WINDOW_EXITING;
	vmcs_write32(VMCS_PRI_PROC_BASED_CTLS, procbased_ctls);
}

unsigned int
vmx_get_reg(unsigned int reg)
{
	unsigned int val = 0;

	switch (reg) {
	case GUEST_EAX:
		val = (uint32_t) vmx.g_rax;
		break;
	case GUEST_EBX:
		val = (uint32_t) vmx.g_rbx;
		break;
	case GUEST_ECX:
		val = (uint32_t) vmx.g_rcx;
		break;
	case GUEST_EDX:
		val = (uint32_t) vmx.g_rdx;
		break;
	case GUEST_ESI:
		val = (uint32_t) vmx.g_rsi;
		break;
	case GUEST_EDI:
		val = (uint32_t) vmx.g_rdi;
		break;
	case GUEST_EBP:
		val = (uint32_t) vmx.g_rbp;
		break;
	case GUEST_ESP:
		val = vmcs_read32(VMCS_GUEST_RSP);
		break;
	case GUEST_EIP:
		val = (uint32_t) vmx.g_rip;
		break;
	case GUEST_EFLAGS:
		val = vmcs_read32(VMCS_GUEST_RFLAGS);
		break;
	case GUEST_CR0:
		val = vmcs_read32(VMCS_GUEST_CR0);
		break;
	case GUEST_CR2:
		val = (uint32_t) vmx.g_cr2;
		break;
	case GUEST_CR3:
		val = vmcs_read32(VMCS_GUEST_CR3);
		break;
	case GUEST_CR4:
		val = vmcs_read32(VMCS_GUEST_CR4);
		break;
	default:
		KERN_PANIC("Unrecognized register %d.\n", reg);
	}

	return val;
}

void
vmx_set_reg(unsigned int reg, unsigned val)
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
        break;
	}
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

void
vmx_set_desc(unsigned int seg, unsigned int sel, unsigned int base, unsigned int lim, unsigned int ar)
{
	vmcs_write32(seg_encode[seg].base, base);
	vmcs_write32(seg_encode[seg].limit, lim);

	if (seg != GUEST_GDTR && seg != GUEST_IDTR) {
		vmcs_write32(seg_encode[seg].access, ar);
		vmcs_write32(seg_encode[seg].selector, sel);
	}
}

void
vmx_set_mmap(unsigned int gpa, unsigned int hpa, unsigned int mem_type)
{
	int rc = 0;

	rc = ept_add_mapping(gpa, hpa, mem_type);

	if (rc) {
		KERN_DEBUG("EPT_add mapping error : ept@0x%x, gpa:0x%x, "
			   "hpa:0x%llx\n", vmx.pml4ept, gpa, hpa);
	} else {
		ept_invalidate_mappings((uint64_t)(uintptr_t) vmx.pml4ept);
	}
}

void
vmx_inject_event(unsigned int type,
		 unsigned int vector, unsigned int errcode, unsigned int ev)
{
	unsigned int intr_info = vmcs_read32(VMCS_ENTRY_INTR_INFO);
	if (intr_info & VMCS_INTERRUPTION_INFO_VALID) {
        KERN_DEBUG("intr_info & VMCS_INTERRUPTION_INFO_VALID is nonzero.\n");
        vmx_dump_info();
        KERN_DEBUG("end of vmx_inject_event error.\n\n\n\n\n\n\n");
		return;
    }

	intr_info = VMCS_INTERRUPTION_INFO_VALID |
		type | vector | ((ev == TRUE ? 1 : 0) << 11);
	vmcs_write32(VMCS_ENTRY_INTR_INFO, intr_info);
	if (ev == 1)
		vmcs_write32(VMCS_ENTRY_EXCEPTION_ERROR, errcode);
}

unsigned int
vmx_get_next_eip()
{
	return vmx.g_rip + vmcs_read32(VMCS_EXIT_INSTRUCTION_LENGTH);
}

unsigned int
vmx_check_pending_event()
{
	return (vmcs_read32(VMCS_ENTRY_INTR_INFO) &
		VMCS_INTERRUPTION_INFO_VALID) ? 1 : 0;
}

unsigned int
vmx_check_int_shadow()
{
	return (vmcs_read32(VMCS_GUEST_INTERRUPTIBILITY) &
		(VMCS_INTERRUPTIBILITY_STI_BLOCKING |
		 VMCS_INTERRUPTIBILITY_MOVSS_BLOCKING)) ? 1 : 0;
}

unsigned int
vmx_get_exit_reason()
{
    unsigned int exit_reason;

    exit_reason = vmcs_read32(VMCS_EXIT_REASON);

    return (exit_reason & EXIT_REASON_MASK);
}

unsigned int
vmx_get_exit_io_port(void)
{
    return EXIT_QUAL_IO_PORT(vmx.exit_qualification);
}

unsigned int
vmx_get_exit_io_width(void)
{
    unsigned int width;

    if (EXIT_QUAL_IO_SIZE(vmx.exit_qualification) == EXIT_QUAL_IO_ONE_BYTE)
        width = SZ8;
    else if (EXIT_QUAL_IO_SIZE(vmx.exit_qualification) == EXIT_QUAL_IO_TWO_BYTE)
        width = SZ16;
    else
        width = SZ32;

    return width;
}

unsigned int
vmx_get_exit_io_write(void)
{
    unsigned int write;

    if (EXIT_QUAL_IO_DIR(vmx.exit_qualification) == EXIT_QUAL_IO_IN)
        write = 0;
    else
        write = 1;

    return write;
}

unsigned int
vmx_get_exit_io_rep(void)
{
    return EXIT_QUAL_IO_REP(vmx.exit_qualification) ? 1 : 0;
}

unsigned int
vmx_get_exit_io_str(void)
{
    return EXIT_QUAL_IO_STR(vmx.exit_qualification) ? 1 : 0;
}

unsigned int
vmx_get_exit_fault_addr(void)
{
    return (uintptr_t) vmcs_read64(VMCS_GUEST_PHYSICAL_ADDRESS);
}

void
vmx_set_tsc_offset(uint64_t tsc_offset)
{
   vmcs_write64(VMCS_TSC_OFFSET, tsc_offset);
}

uint64_t
vmx_get_tsc_offset(void)
{
   return vmcs_read64(VMCS_TSC_OFFSET);
}

void
vmx_run_vm()
{
    vmptrld(vmx.vmcs);

    vmcs_write32(VMCS_GUEST_RIP, vmx.g_rip);

    /* the address of vmx is stored in %ecx */
    __asm __volatile(
             /* save host registers */
             "pushl %%ebp;"
             "pushl %%edi;"
             /* save the address of vmx on the stack */
             "pushl %%ecx;"         /* placeholder */
             "pushl %%ecx;"         /* address of vmx */
             "movl %1, %%edi;"
             "vmwrite %%esp, %%edi;"
             /* save entry TSC */
             "pushl %%ecx;"
             "rdtscp;"
             "popl %%ecx;"
             "movl %%eax, %c[enter_tsc_lo](%0);"
             "movl %%edx, %c[enter_tsc_hi](%0);"
             /* load guest registers */
             "movl %c[g_cr2](%0), %%edi;"   /* guest %cr2 */
             "movl %%edi, %%cr2;"
             "movl %c[g_dr0](%0), %%edi;"   /* guest %dr0 */
             "movl %%edi, %%dr0;"
             "movl %c[g_dr1](%0), %%edi;"   /* guest %dr1 */
             "movl %%edi, %%dr1;"
             "movl %c[g_dr2](%0), %%edi;"   /* guest %dr2 */
             "movl %%edi, %%dr2;"
             "movl %c[g_dr3](%0), %%edi;"   /* guest %dr3 */
             "movl %%edi, %%dr3;"
             "movl %c[g_dr6](%0), %%edi;"   /* guest %dr6 */
             /* "movl %%edi, %%dr6;" */
             "movl %c[g_rax](%0), %%eax;"   /* guest %eax */
             "movl %c[g_rbx](%0), %%ebx;"   /* guest %ebx */
             "movl %c[g_rdx](%0), %%edx;"   /* guest %edx */
             "movl %c[g_rsi](%0), %%esi;"   /* guest %esi */
             "movl %c[g_rdi](%0), %%edi;"   /* guest %edi */
             "movl %c[g_rbp](%0), %%ebp;"   /* guest %ebp */
             /* launch/resume the guest */
             "cmpl $0, %c[launched](%0); jz .Lvmx_launch;"
             "movl %c[g_rcx](%0), %%ecx;"   /* guest %ecx */
             "vmresume; jmp vmx_return_from_guest;"
             ".Lvmx_launch:;"
             "movl %c[g_rcx](%0), %%ecx;"   /* guest %ecx */
             "vmlaunch;"
             "vmx_return_from_guest:"
             /* save guest registers */
             "movl %%ecx, 4(%%esp);"    /* temporally save guest
                               %ecx in placeholder*/
             "popl %%ecx;"          /* reload vmx */
             /* check error first */
             "jnc 1f;"
             "movl $1, %c[failed](%0);" /* CF: error = 1 */
             "jmp 3f;"
             "1: jnz 2f;"
             "movl $2, %c[failed](%0);" /* ZF: error = 2 */
             "jmp 3f;"
             "2: movl $0, %c[failed](%0);"
             "3: nop;"
             /* save guest */
             "movl %%edi, %c[g_rdi](%0);"   /* guest %edi */
             "movl %%cr2, %%edi;"       /* guest %cr2 */
             "movl %%edi, %c[g_cr2](%0);"
             "movl %%dr0, %%edi;"       /* guest %dr0 */
             "movl %%edi, %c[g_dr0](%0);"
             "movl %%dr1, %%edi;"       /* guest %dr1 */
             "movl %%edi, %c[g_dr1](%0);"
             "movl %%dr2, %%edi;"       /* guest %dr2 */
             "movl %%edi, %c[g_dr1](%0);"
             "movl %%dr3, %%edi;"       /* guest %dr3 */
             "movl %%edi, %c[g_dr3](%0);"
             "movl %%dr6, %%edi;"       /* guest %dr6 */
             "movl %%edi, %c[g_dr6](%0);"
             "movl %%eax, %c[g_rax](%0);"   /* guest %eax */
             "movl %%ebx, %c[g_rbx](%0);"   /* guest %ebx */
             "popl %%edi;"          /* guest %ecx */
             "movl %%edi, %c[g_rcx](%0);"
             "movl %%edx, %c[g_rdx](%0);"   /* guest %edx */
             "movl %%esi, %c[g_rsi](%0);"   /* guest %esi */
             "movl %%ebp, %c[g_rbp](%0);"   /* guest %ebp */
             /* save exit TSC */
             "pushl %%ecx;"
             "rdtscp;"
             "popl %%ecx;"
             "movl %%eax, %c[exit_tsc_lo](%0);"
             "movl %%edx, %c[exit_tsc_hi](%0);"
             /* load host registers */
             "popl %%edi;"
             "popl %%ebp;"
             : : "c" (&vmx), "i" (VMCS_HOST_RSP),
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

    if (unlikely(vmx.failed == 1)) {
        KERN_DEBUG("vmlaunch/vmresume failed: error %d.\n",
               vmx.failed);
        return;
    } else if (unlikely(vmx.failed == 2)) {
        KERN_DEBUG("vmlaunch/vmresume failed: error %d, code 0x%08x.\n",
               vmx.failed, vmcs_read32(VMCS_INSTRUCTION_ERROR));
        return;
    }

    vmx.g_rip = vmcs_read32(VMCS_GUEST_RIP);
    vmx.exit_reason = vmcs_read32(VMCS_EXIT_REASON);
    vmx.exit_qualification = vmcs_read32(VMCS_EXIT_QUALIFICATION);

    if (unlikely(vmx.exit_reason & EXIT_REASON_ENTRY_FAIL)) {
        KERN_DEBUG("VM-entry failure: reason %d, qualification %d.\n",
               vmx.exit_reason & 0x0000ffff);
        vmx_dump_info();
        KERN_DEBUG("end of vmx_entry error.\n\n\n\n\n\n\n");
        return;
    }

    vmx.launched = 1;
}

extern struct vmcs vmcs;
extern struct eptStruct ept;
extern void sched_init(unsigned int);
extern void vmcs_set_defaults(struct vmcs *, uint64_t *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, char *, char *, char *, uint16_t, uint64_t, uint64_t, uint64_t, uint64_t, uintptr_t);

void
dump_vmx_info(void)
{
    VMX_DEBUG("pin-based ctls 0x%08x\n", vmx_info.pinbased_ctls);
    VMX_DEBUG("primary processor-based ctls 0x%08x\n",
                      vmx_info.procbased_ctls);
    VMX_DEBUG("secondary processor-based ctls 0x%08x\n",
                      vmx_info.procbased_ctls2);
    VMX_DEBUG("exit ctls 0x%08x\n", vmx_info.exit_ctls);
    VMX_DEBUG("entry ctls 0x%08x\n", vmx_info.entry_ctls);
    VMX_DEBUG("CR0 1s mask 0x%llx, 0s mask 0x%llx.\n",
                      vmx_info.cr0_ones_mask, vmx_info.cr0_zeros_mask);
    VMX_DEBUG("CR4 1s mask 0x%llx, 0s mask 0x%llx.\n",
                      vmx_info.cr4_ones_mask, vmx_info.cr4_zeros_mask);
}

void
vmx_init(unsigned int mbi_addr)
{
    extern uint8_t vmx_return_from_guest[];
    int rw;

    KERN_DEBUG("In vmx init.\n");

    sched_init(mbi_addr);

    VMX_DEBUG("Before vmx_hw_init.\n");
    dump_vmx_info();
    vmx_hw_init();
    VMX_DEBUG("\n\nAfter vmx_hw_init.\n");
    dump_vmx_info();
    KERN_DEBUG("vmx hw initialized.\n");

    memset(&vmcs, 0, sizeof(vmcs));
    memset(msr_bitmap, 0, sizeof(msr_bitmap));
    memset(io_bitmap, 0, sizeof(io_bitmap));

    // intercept all io ports
    uint32_t *bitmap = (uint32_t *) io_bitmap;
    memset(bitmap, 0xff, PAGESIZE * 2);

    // intercept all msrs
    rw = 0;
    char *rdmsr_bitmap = msr_bitmap;
    char *wrmsr_bitmap = (char *) ((uintptr_t) msr_bitmap + 0x800);

    memset(rdmsr_bitmap, (rw & 0x1) ? 0xf : 0x0, 2048);
    memset(wrmsr_bitmap, (rw & 0x2) ? 0xf : 0x0, 2048);

    vmx.vmcs = &vmcs;
    vmx.pml4ept = ept.pml4;
    vmx.msr_bitmap = msr_bitmap;
    vmx.io_bitmap = io_bitmap;
    vmx.vmcs->identifier = rdmsr(MSR_VMX_BASIC) & 0xffffffff;
    vmx.g_rip = 0xfff0;
    vmcs_set_defaults
        (vmx.vmcs, vmx.pml4ept, vmx_info.pinbased_ctls,
         vmx_info.procbased_ctls, vmx_info.procbased_ctls2,
         vmx_info.exit_ctls, vmx_info.entry_ctls, vmx.msr_bitmap,
         vmx.io_bitmap, (char *) ((uintptr_t) vmx.io_bitmap+PAGESIZE),
         1,
         vmx_info.cr0_ones_mask, vmx_info.cr0_zeros_mask,
         vmx_info.cr4_ones_mask, vmx_info.cr4_zeros_mask,
         (uintptr_t) vmx_return_from_guest);
    vmx.vpid = 1;
    vmx.g_cr2 = 0;
    vmx.g_dr0 = vmx.g_dr1 = vmx.g_dr2 = vmx.g_dr3 = 0;
    vmx.g_dr6 = 0xffff0ff0;

}

static bool
vmx_ctl_allows_one_setting(uint64_t msr_val, int bitpos)
{

    if (msr_val & (1ULL << (bitpos + 32)))
        return (TRUE);
    else
        return (FALSE);
}

static bool
vmx_ctl_allows_zero_setting(uint64_t msr_val, int bitpos)
{

    if ((msr_val & (1ULL << bitpos)) == 0)
        return (TRUE);
    else
        return (FALSE);
}

int
vmx_set_ctlreg(int ctl_reg, int true_ctl_reg, uint32_t ones_mask,
           uint32_t zeros_mask, uint32_t *retval)
{
    int i;
    uint64_t val, trueval;
    bool true_ctls_avail, one_allowed, zero_allowed;

    /* We cannot ask the same bit to be set to both '1' and '0' */
    if ((ones_mask ^ zeros_mask) != (ones_mask | zeros_mask)) {
#ifdef DEBUG_GUEST_MSR
        VMX_DEBUG("0's mask 0x%08x and 1's mask 0x%08x are "
              "overlapped.\n", zeros_mask, ones_mask);
#endif
        return 1;
    }

    if (rdmsr(MSR_VMX_BASIC) & (1ULL << 55))
        true_ctls_avail = TRUE;
    else
        true_ctls_avail = FALSE;

    val = rdmsr(ctl_reg);
    if (true_ctls_avail)
        trueval = rdmsr(true_ctl_reg);      /* step c */
    else
        trueval = val;              /* step a */

    for (i = 0; i < 32; i++) {
        one_allowed = vmx_ctl_allows_one_setting(trueval, i);
        zero_allowed = vmx_ctl_allows_zero_setting(trueval, i);

        KERN_ASSERT(one_allowed || zero_allowed);

        if (zero_allowed && !one_allowed) {     /* b(i),c(i) */
            if (ones_mask & (1 << i)) {
#ifdef DEBUG_GUEST_MSR
                VMX_DEBUG("Cannot set bit %d to zero.\n", i);
#endif
                return 1;
            }
            *retval &= ~(1 << i);
        } else if (one_allowed && !zero_allowed) {  /* b(i),c(i) */
            if (zeros_mask & (1 << i)) {
#ifdef DEBUG_GUEST_MSR
                VMX_DEBUG("Cannot set bit %d to one.\n", i);
#endif
                return 2;
            }
            *retval |= 1 << i;
        } else {
            if (zeros_mask & (1 << i))  /* b(ii),c(ii) */
                *retval &= ~(1 << i);
            else if (ones_mask & (1 << i)) /* b(ii), c(ii) */
                *retval |= 1 << i;
            else if (!true_ctls_avail)
                *retval &= ~(1 << i);   /* b(iii) */
            else if (vmx_ctl_allows_zero_setting(val, i))/* c(iii)*/
                *retval &= ~(1 << i);
            else if (vmx_ctl_allows_one_setting(val, i)) /* c(iv) */
                *retval |= 1 << i;
            else {
                KERN_PANIC("Cannot determine the value of bit %d "
                       "of MSR 0x%08x and true MSR 0x%08x.\n",
                       i, ctl_reg, true_ctl_reg);
            }
        }
    }

    return (0);
}
