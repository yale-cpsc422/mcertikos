#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/mem.h>
#include <sys/pcpu.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/hvm.h>

#include "svm.h"
#include "svm_drv.h"

static struct svm svm0;

#ifdef __COMPCERT__
static void init_seg_offset(void);
static void init_svm_event_type(void);
#endif

/*
 * Allocate one memory page for the host state-save area.
 */
static uintptr_t
alloc_hsave_area(void)
{
	pageinfo_t *pi = mem_page_alloc();

	if (pi == NULL) {
		SVM_DEBUG("Failed to allocate memory for "
			  "the host state-save area failed.\n");
		return 0x0;
	}

	return mem_pi2phys(pi);
}

/*
 * Set an interception bit in VMCB.
 *
 * @param vmcb the pointer to a VMCB
 * @param bit the interception bit to be set
 */
static void
set_intercept(struct vmcb *vmcb, int bit, bool enable)
{
	KERN_ASSERT(vmcb != NULL);
	KERN_ASSERT(INTERCEPT_INTR <= bit && bit <= INTERCEPT_XSETBV);

#ifndef __COMPCERT__
	if (enable == TRUE)
		vmcb->control.intercept |= (1ULL << bit);
	else
		vmcb->control.intercept &= ~(1ULL << bit);
#else
	if (enable == TRUE) {
		if (bit < 32)
			vmcb->control.intercept_lo |= (1UL << bit);
		else
			vmcb->control.intercept_hi |= (1UL << (bit - 32));
	} else {
		if (bit < 32)
			vmcb->control.intercept_lo &= ~(1UL << bit);
		else
			vmcb->control.intercept_hi &= ~(1UL << (bit - 32));
	}
#endif
}

static int
svm_handle_exit(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_control_area *ctrl = &vmcb->control;
#ifndef __COMPCERT__
	uint32_t exitinfo1 = ctrl->exit_info_1;
#else
	uint32_t exitinfo1 = ctrl->exit_info_1_lo;
	uint32_t exitinfo2 = ctrl->exit_info_2_lo;
#endif

#ifdef DEBUG_VMEXIT
#ifndef __COMPCERT__
	SVM_DEBUG("VMEXIT exit_code 0x%x, guest EIP 0x%08x.\n",
		  ctrl->exit_code, (uintptr_t) vmcb->save.rip);
#else
	SVM_DEBUG("VMEXIT exit_code 0x%x, guest EIP 0x%08x.\n",
		  ctrl->exit_code, vmcb->save.rip_lo);
#endif
#endif

	switch (ctrl->exit_code) {
	case SVM_EXIT_INTR:
		vm->exit_reason = EXIT_REASON_EXTINT;
		break;

	case SVM_EXIT_VINTR:
		vm->exit_reason = EXIT_REASON_INTWIN;
		ctrl->int_ctl &= ~SVM_INTR_CTRL_VIRQ;
		break;

	case SVM_EXIT_IOIO:
		vm->exit_reason = EXIT_REASON_IOPORT;
		vm->exit_info.ioport.port =
			(exitinfo1 & SVM_EXITINFO1_PORT_MASK) >>
			SVM_EXITINFO1_PORT_SHIFT;
		vm->exit_info.ioport.width =
			(exitinfo1 & SVM_EXITINFO1_SZ8) ? SZ8 :
			(exitinfo1 & SVM_EXITINFO1_SZ16) ? SZ16 : SZ32;
		vm->exit_info.ioport.write =
			(exitinfo1 & SVM_EXITINFO1_TYPE_IN) ? FALSE : TRUE;
		vm->exit_info.ioport.rep =
			(exitinfo1 & SVM_EXITINFO1_REP) ? TRUE : FALSE;
		vm->exit_info.ioport.str =
			(exitinfo1 & SVM_EXITINFO1_STR) ? TRUE : FALSE;
		break;

	case SVM_EXIT_NPF:
		vm->exit_reason = EXIT_REASON_PGFLT;
#ifndef __COMPCERT__
		vm->exit_info.pgflt.addr = (uintptr_t) PGADDR(ctrl->exit_info_2);
#else
		vm->exit_info.pgflt.addr = (uintptr_t) PGADDR(exitinfo2);
#endif
		KERN_DEBUG("NPT fault @ 0x%08x.\n", vm->exit_info.pgflt.addr);
		break;

	case SVM_EXIT_CPUID:
		vm->exit_reason = EXIT_REASON_CPUID;
		break;

	case SVM_EXIT_RDTSC:
		vm->exit_reason = EXIT_REASON_RDTSC;
		break;

	case SVM_EXIT_VMMCALL:
	case SVM_EXIT_HLT:
	case SVM_EXIT_MSR:
	case SVM_EXIT_VMRUN:
	case SVM_EXIT_VMLOAD:
	case SVM_EXIT_VMSAVE:
	case SVM_EXIT_STGI:
	case SVM_EXIT_CLGI:
	case SVM_EXIT_SKINIT:
	case SVM_EXIT_RDTSCP:
	case SVM_EXIT_MONITOR:
	case SVM_EXIT_MWAIT:
	case SVM_EXIT_MWAIT_COND:
		vm->exit_reason = EXIT_REASON_INVAL_INSTR;
		break;

	default:
		vm->exit_reason = EXIT_REASON_INVAL;
	}

	return 0;
}

/*
 * Initialize SVM module.
 * - check whether SVM is supported
 * - enable SVM
 * - allocate host save area
 *
 * @return 0 if succeed
 */
static int
svm_init(void)
{
	memzero(&svm0, sizeof(svm0));
	svm0.inuse = 0;

#ifdef __COMPCERT__
	init_seg_offset();
	init_svm_event_type();
#endif

	uintptr_t hsave_addr = alloc_hsave_area();

	if (hsave_addr == 0x0)
		return 1;
	else
		SVM_DEBUG("Host state-save area is at %x.\n", hsave_addr);

	return svm_drv_init(hsave_addr);
}

static int
svm_init_vm(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	int rc;
	struct svm *svm = &svm0;
	pageinfo_t *vmcb_pi, *iopm_pi, *msrpm_pi, *ncr3_pi;

	/*
	 * Allocate memory.
	 */

	if (svm->inuse == 1) {
		rc = -1;
		goto err0;
	}
	memzero(svm, sizeof(struct svm));
	svm->inuse = 1;
	vm->cookie = svm;

	if ((vmcb_pi = mem_page_alloc()) == NULL) {
		rc = -2;
		goto err1;
	}
	svm->vmcb = mem_pi2ptr(vmcb_pi);
	memzero(svm->vmcb, PAGESIZE);

	if ((iopm_pi = mem_pages_alloc(SVM_IOPM_SIZE)) == NULL) {
		rc = -3;
		goto err2;
	}

#ifndef __COMPCERT__
	svm->vmcb->control.iopm_base_pa = mem_pi2phys(iopm_pi);
#else
	svm->vmcb->control.iopm_base_pa_lo = mem_pi2phys(iopm_pi);
	svm->vmcb->control.iopm_base_pa_hi = 0;
#endif

	if ((msrpm_pi = mem_pages_alloc(SVM_MSRPM_SIZE)) == NULL) {
		rc = -4;
		goto err3;
	}
#ifndef __COMPCERT__
	svm->vmcb->control.msrpm_base_pa = mem_pi2phys(msrpm_pi);
#else
	svm->vmcb->control.msrpm_base_pa_lo = mem_pi2phys(msrpm_pi);
	svm->vmcb->control.msrpm_base_pa_hi = 0;
#endif

	if ((ncr3_pi = mem_page_alloc()) == NULL) {
		rc = -5;
		goto err4;
	}
#ifndef __COMPCERT__
	svm->vmcb->control.nested_cr3 = mem_pi2phys(ncr3_pi);
	memzero((void *)(uint32_t) svm->vmcb->control.nested_cr3, PAGESIZE);
#else
	svm->vmcb->control.nested_cr3_lo = mem_pi2phys(ncr3_pi);
	svm->vmcb->control.nested_cr3_hi = 0;
	memzero((void *) svm->vmcb->control.nested_cr3_lo, PAGESIZE);
#endif

	/*
	 * Setup default interception.
	 */

	/* intercept all I/O ports */
#ifndef __COMPCERT__
	memset((void *)(uint32_t) svm->vmcb->control.iopm_base_pa,
	       0xf, SVM_IOPM_SIZE);
	/* do not intercept any MSR */
	memzero((void *)(uint32_t) svm->vmcb->control.msrpm_base_pa,
		SVM_MSRPM_SIZE);
#else
	memset((void *) svm->vmcb->control.iopm_base_pa_lo,
	       0xf, SVM_IOPM_SIZE);
	/* do not intercept any MSR */
	memzero((void *) svm->vmcb->control.msrpm_base_pa_lo,
		SVM_MSRPM_SIZE);
#endif

	/* intercept instructions */
	set_intercept(svm->vmcb, INTERCEPT_VMRUN, TRUE);	/* vmrun */
	set_intercept(svm->vmcb, INTERCEPT_VMMCALL, TRUE);	/* vmmcall */
	set_intercept(svm->vmcb, INTERCEPT_STGI, TRUE);		/* stgi */
	set_intercept(svm->vmcb, INTERCEPT_CLGI, TRUE);		/* clgi */
	set_intercept(svm->vmcb, INTERCEPT_IOIO_PROT, TRUE);	/* in/out */
	set_intercept(svm->vmcb, INTERCEPT_CPUID, TRUE);	/* cpuid */
	set_intercept(svm->vmcb, INTERCEPT_RDTSC, TRUE);	/* rdtsc */
	set_intercept(svm->vmcb, INTERCEPT_RDTSCP, TRUE);	/* rdtscp */

	/* intercept interrupts */
	set_intercept(svm->vmcb, INTERCEPT_INTR, TRUE);

	/*
	 * Setup initial state.
	 */

#ifndef __COMPCERT__
	svm->vmcb->save.cr0 = 0x60000010;
	svm->vmcb->save.cr2 = svm->vmcb->save.cr3 = svm->vmcb->save.cr4 = 0;
	svm->vmcb->save.dr6 = 0xffff0ff0;
	svm->vmcb->save.dr7 = 0x00000400;
	svm->vmcb->save.efer = MSR_EFER_SVME;
	svm->vmcb->save.g_pat = 0x7040600070406ULL;
#else
	svm->vmcb->save.cr0_lo = 0x60000010;
	svm->vmcb->save.cr0_hi = 0;
	svm->vmcb->save.cr2_lo = svm->vmcb->save.cr3_lo = svm->vmcb->save.cr4_lo = 0;
	svm->vmcb->save.cr2_hi = svm->vmcb->save.cr3_hi = svm->vmcb->save.cr4_hi = 0;
	svm->vmcb->save.dr6_lo = 0xffff0ff0;
	svm->vmcb->save.dr6_hi = 0;
	svm->vmcb->save.dr7_lo = 0x00000400;
	svm->vmcb->save.dr7_hi = 0;
	svm->vmcb->save.efer_lo = MSR_EFER_SVME;
	svm->vmcb->save.efer_hi = 0;
	svm->vmcb->save.g_pat_lo = 0x00070406;
	svm->vmcb->save.g_pat_hi = 0x00070406;
#endif

	svm->vmcb->control.asid = 1;
#ifndef __COMPCERT__
	svm->vmcb->control.nested_ctl = 1;
#else
	svm->vmcb->control.nested_ctl_lo = 1;
	svm->vmcb->control.nested_ctl_hi = 0;
#endif
	svm->vmcb->control.tlb_ctl = 0;
#ifndef __COMPCERT__
	svm->vmcb->control.tsc_offset = 0;
#else
	svm->vmcb->control.tsc_offset_lo = 0;
	svm->vmcb->control.tsc_offset_hi = 0;
#endif
	/* Sec 15.21.1, APM Vol2 r3.19 */
	svm->vmcb->control.int_ctl =
		(SVM_INTR_CTRL_VINTR_MASK | (0x0 & SVM_INTR_CTRL_VTPR));

	return 0;

 err4:
	mem_pages_free(msrpm_pi);
 err3:
	mem_pages_free(iopm_pi);
 err2:
	mem_pages_free(vmcb_pi);
 err1:
	svm->inuse = 0;
 err0:
	return rc;
}

static int
svm_run_vm(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_control_area *ctrl = &vmcb->control;

	svm_drv_run_vm(svm);

	if (ctrl->exit_int_info & SVM_EXITINTINFO_VALID) {
		uint32_t exit_int_info = ctrl->exit_int_info;
		uint32_t errcode = ctrl->exit_int_info_err;
		uint32_t int_type = exit_int_info & SVM_EXITINTINFO_TYPE_MASK;

		switch (int_type) {
		case SVM_EXITINTINFO_TYPE_INTR:
#ifdef DEBUG_GUEST_INTR
			SVM_DEBUG("Pending INTR: vec=%x.\n",
				  exit_int_info & SVM_EXITINTINFO_VEC_MASK);
#endif
			ctrl->event_inj = exit_int_info;
			ctrl->event_inj_err = errcode;
			break;

		case SVM_EXITINTINFO_TYPE_NMI:
#ifdef DEBUG_GUEST_INTR
			SVM_DEBUG("Pending NMI.\n");
#endif
			ctrl->event_inj = exit_int_info;
			ctrl->event_inj_err = errcode;
			break;

		default:
			break;
		}
	}

	if (ctrl->exit_code == SVM_EXIT_ERR) {
		svm_handle_err(vmcb);
		KERN_PANIC("SVM_EXIT_ERR");
	}

	return svm_handle_exit(vm);
}

static int
svm_intercept_ioport(struct vm *vm, uint16_t port, bool enable)
{
	KERN_ASSERT(vm != NULL);

#ifdef DEBUG_GUEST_IOPORT
	SVM_DEBUG("%s intercepting guest I/O port 0x%x.\n",
		  (enable == TRUE) ? "Enable" : "Disable", port);
#endif

	struct svm *svm = (struct svm *) vm->cookie;
#ifndef __COMPCERT__
	uint32_t *iopm = (uint32_t *)(uintptr_t) svm->vmcb->control.iopm_base_pa;
#else
	uint32_t *iopm = (uint32_t *)(uintptr_t)
		svm->vmcb->control.iopm_base_pa_lo;
#endif

	int entry = port / 32;
	int bit = port - entry * 32;

	if (enable == TRUE)
		iopm[entry] |= (1 << bit);
	else
		iopm[entry] &= ~(uint32_t) (1 << bit);

	return 0;
}

static int
svm_intercept_msr(struct vm *vm, uint32_t msr, int rw)
{
	KERN_PANIC("svm_intercept_msr() not implemented yet.\n");
	return 1;
}

static void
svm_inject_vintr(struct vmcb *vmcb, uint8_t vector, uint8_t priority)
{
	KERN_ASSERT(vmcb != NULL);

	struct vmcb_control_area *ctrl  = &vmcb->control;

	ctrl->int_ctl |= SVM_INTR_CTRL_VIRQ |
		((priority << SVM_INTR_CTRL_PRIO_SHIFT) & SVM_INTR_CTRL_PRIO) |
		SVM_INTR_CTRL_IGN_VTPR;
	ctrl->int_vector = vector;
}

static int
svm_intercept_vintr(struct vm *vm, bool enable)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;

	if (enable == TRUE) {
		svm_inject_vintr(vmcb, 0, 0);
		set_intercept(vmcb, INTERCEPT_VINTR, TRUE);
	} else {
		set_intercept(vmcb, INTERCEPT_VINTR, FALSE);
	}

	return 0;
}

static int
svm_get_reg(struct vm *vm, guest_reg_t reg, uint32_t *val)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(val != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_save_area *save = &svm->vmcb->save;

	switch (reg) {
	case GUEST_EAX:
#ifndef __COMPCERT__
		*val = (uint32_t) save->rax;
#else
		*val = save->rax_lo;
#endif
		break;
	case GUEST_EBX:
		*val = (uint32_t) svm->g_rbx;
		break;
	case GUEST_ECX:
		*val = (uint32_t) svm->g_rcx;
		break;
	case GUEST_EDX:
		*val = (uint32_t) svm->g_rdx;
		break;
	case GUEST_ESI:
		*val = (uint32_t) svm->g_rsi;
		break;
	case GUEST_EDI:
		*val = (uint32_t) svm->g_rdi;
		break;
	case GUEST_EBP:
		*val = (uint32_t) svm->g_rbp;
		break;
	case GUEST_ESP:
#ifndef __COMPCERT__
		*val = (uint32_t) save->rsp;
#else
		*val = save->rsp_lo;
#endif
		break;
	case GUEST_EIP:
#ifndef __COMPCERT__
		*val = (uint32_t) save->rip;
#else
		*val = save->rip_lo;
#endif
		break;
	case GUEST_EFLAGS:
#ifndef __COMPCERT__
		*val = (uint32_t) save->rflags;
#else
		*val = save->rflags_lo;
#endif
		break;
	case GUEST_CR0:
#ifndef __COMPCERT__
		*val = (uint32_t) save->cr0;
#else
		*val = save->cr0_lo;
#endif
		break;
	case GUEST_CR2:
#ifndef __COMPCERT__
		*val = (uint32_t) save->cr2;
#else
		*val = save->cr2_lo;
#endif
		break;
	case GUEST_CR3:
#ifndef __COMPCERT__
		*val = (uint32_t) save->cr3;
#else
		*val = save->cr3_lo;
#endif
		break;
	case GUEST_CR4:
#ifndef __COMPCERT__
		*val = (uint32_t) save->cr4;
#else
		*val = save->cr4_lo;
#endif
		break;
	default:
		return 1;
	}

	return 0;
}

static int
svm_set_reg(struct vm *vm, guest_reg_t reg, uint32_t val)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_save_area *save = &svm->vmcb->save;

	switch (reg) {
	case GUEST_EAX:
#ifndef __COMPCERT__
		save->rax = val;
#else
		save->rax_lo = val;
		save->rax_hi = 0;
#endif
		break;
	case GUEST_EBX:
		svm->g_rbx = val;
		break;
	case GUEST_ECX:
		svm->g_rcx = val;
		break;
	case GUEST_EDX:
		svm->g_rdx = val;
		break;
	case GUEST_ESI:
		svm->g_rsi = val;
		break;
	case GUEST_EDI:
		svm->g_rdi = val;
		break;
	case GUEST_EBP:
		svm->g_rbp = val;
		break;
	case GUEST_ESP:
#ifndef __COMPCERT__
		save->rsp = val;
#else
		save->rsp_lo = val;
		save->rsp_hi = 0;
#endif
		break;
	case GUEST_EIP:
#ifndef __COMPCERT__
		save->rip = val;
#else
		save->rip_lo = val;
		save->rip_hi = 0;
#endif
		break;
	case GUEST_EFLAGS:
#ifndef __COMPCERT__
		save->rflags = val;
#else
		save->rflags_lo = val;
		save->rflags_hi = 0;
#endif
		break;
	case GUEST_CR0:
#ifndef __COMPCERT__
		save->cr0 = val;
#else
		save->cr0_lo = val;
		save->cr0_hi = 0;
#endif
		break;
	case GUEST_CR2:
#ifndef __COMPCERT__
		save->cr2 = val;
#else
		save->cr2_lo = val;
		save->cr2_hi = 0;
#endif
		break;
	case GUEST_CR3:
#ifndef __COMPCERT__
		save->cr3 = val;
#else
		save->cr3_lo = val;
		save->cr3_hi = 0;
#endif
		break;
	case GUEST_CR4:
#ifndef __COMPCERT__
		save->cr4 = val;
#else
		save->cr4_lo = val;
		save->cr4_hi = 0;
#endif
		break;
	default:
		return 1;
	}

	return 0;
}

#ifndef __COMPCERT__

static uintptr_t seg_offset[10] = {
	[GUEST_CS]	= offsetof(struct vmcb_save_area, cs),
	[GUEST_DS]	= offsetof(struct vmcb_save_area, ds),
	[GUEST_ES]	= offsetof(struct vmcb_save_area, es),
	[GUEST_FS]	= offsetof(struct vmcb_save_area, fs),
	[GUEST_GS]	= offsetof(struct vmcb_save_area, gs),
	[GUEST_SS]	= offsetof(struct vmcb_save_area, ss),
	[GUEST_LDTR]	= offsetof(struct vmcb_save_area, ldtr),
	[GUEST_TR]	= offsetof(struct vmcb_save_area, tr),
	[GUEST_GDTR]	= offsetof(struct vmcb_save_area, gdtr),
	[GUEST_IDTR]	= offsetof(struct vmcb_save_area, idtr),
};

#else

static uintptr_t seg_offset[10];

static void
init_seg_offset(void)
{
	seg_offset[GUEST_CS]	= offsetof(struct vmcb_save_area, cs);
	seg_offset[GUEST_DS]	= offsetof(struct vmcb_save_area, ds);
	seg_offset[GUEST_ES]	= offsetof(struct vmcb_save_area, es);
	seg_offset[GUEST_FS]	= offsetof(struct vmcb_save_area, fs);
	seg_offset[GUEST_GS]	= offsetof(struct vmcb_save_area, gs);
	seg_offset[GUEST_SS]	= offsetof(struct vmcb_save_area, ss);
	seg_offset[GUEST_LDTR]	= offsetof(struct vmcb_save_area, ldtr);
	seg_offset[GUEST_TR]	= offsetof(struct vmcb_save_area, tr);
	seg_offset[GUEST_GDTR]	= offsetof(struct vmcb_save_area, gdtr);
	seg_offset[GUEST_IDTR]	= offsetof(struct vmcb_save_area, idtr);
}

#endif

static int
svm_get_desc(struct vm *vm, guest_seg_t seg, struct guest_seg_desc *desc)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(desc != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_save_area *save = &svm->vmcb->save;
	struct vmcb_seg *vmcb_seg =
		(struct vmcb_seg *) ((uintptr_t) save + seg_offset[seg]);

	desc->sel = vmcb_seg->selector;
#ifndef __COMPCERT__
	desc->base = vmcb_seg->base;
#else
	desc->base_lo = vmcb_seg->base_lo;
	desc->base_hi = vmcb_seg->base_hi;
#endif
	desc->lim = vmcb_seg->limit;
	desc->ar =
		(vmcb_seg->attrib & 0xff) | ((vmcb_seg->attrib & 0xf00) << 4);

	return 0;
}

static int
svm_set_desc(struct vm *vm, guest_seg_t seg, struct guest_seg_desc *desc)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(desc != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_save_area *save = &svm->vmcb->save;
	struct vmcb_seg *vmcb_seg =
		(struct vmcb_seg *) ((uintptr_t) save + seg_offset[seg]);

	vmcb_seg->selector = desc->sel;
#ifndef __COMPCERT__
	vmcb_seg->base = desc->base;
#else
	vmcb_seg->base_lo = desc->base_lo;
	vmcb_seg->base_hi = desc->base_hi;
#endif
	vmcb_seg->limit = desc->lim;
	vmcb_seg->attrib = (desc->ar & 0xff) | ((desc->ar & 0xf000) >> 4);

	return 0;
}

static int
svm_set_mmap(struct vm *vm, uintptr_t gpa, uintptr_t hpa, int type)
{
	KERN_ASSERT(vm != NULL);

	if (ROUNDDOWN(gpa, PAGESIZE) != gpa || ROUNDDOWN(hpa, PAGESIZE) != hpa)
		return 1;

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_control_area *ctrl = &svm->vmcb->control;
#ifndef __COMPCERT__
	pmap_t *npt = (pmap_t *)(uintptr_t) ctrl->nested_cr3;
#else
	pmap_t *npt = (pmap_t *)(uintptr_t) ctrl->nested_cr3_lo;
#endif

	npt = pmap_insert(npt, mem_phys2pi(hpa), gpa, PTE_W | PTE_G | PTE_U);

	return (npt == NULL) ? 2 : 0;
}

#ifndef __COMPCERT__

static int svm_event_type[4] = {
	[EVENT_EXTINT]		= SVM_EVTINJ_TYPE_INTR,
	[EVENT_NMI]		= SVM_EVTINJ_TYPE_NMI,
	[EVENT_EXCEPTION]	= SVM_EVTINJ_TYPE_EXEPT,
	[EVENT_SWINT]		= SVM_EVTINJ_TYPE_SOFT
};

#else

static int svm_event_type[4];

static void
init_svm_event_type(void)
{
	svm_event_type[EVENT_EXTINT]	= SVM_EVTINJ_TYPE_INTR;
	svm_event_type[EVENT_NMI]	= SVM_EVTINJ_TYPE_NMI;
	svm_event_type[EVENT_EXCEPTION]	= SVM_EVTINJ_TYPE_EXEPT;
	svm_event_type[EVENT_SWINT]	= SVM_EVTINJ_TYPE_SOFT;
}

#endif

static int
svm_inject_event(struct vm *vm,
		 guest_event_t type, uint8_t vector, uint32_t errcode, bool ev)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_control_area *ctrl = &svm->vmcb->control;

	if (ctrl->event_inj & SVM_EVTINJ_VALID)
		return 1;

	ctrl->event_inj = SVM_EVTINJ_VALID | (vector & SVM_EVTINJ_VEC_MASK) |
		((svm_event_type[type] << SVM_EVTINJ_TYPE_SHIFT) &
		 SVM_EVTINJ_TYPE_MASK);
	ctrl->event_inj_err = errcode;

	if (ev == TRUE)
		ctrl->event_inj |= SVM_EVTINJ_VALID_ERR;

	return 0;
}

static int
svm_get_next_eip(struct vm *vm, guest_instr_t instr, uint32_t *val)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(val != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_control_area *ctrl = &svm->vmcb->control;
	struct vmcb_save_area *save = &svm->vmcb->save;

	switch (instr) {
	case INSTR_IN:
	case INSTR_OUT:
#ifndef __COMPCERT__
		*val = (uint32_t) ctrl->exit_info_2;
#else
		*val = ctrl->exit_info_2_lo;
#endif
		break;
	case INSTR_RDMSR:
	case INSTR_WRMSR:
	case INSTR_CPUID:
	case INSTR_RDTSC:
#ifndef __COMPCERT__
		*val = (uint32_t) save->rip + 2;
#else
		*val = save->rip_lo + 2;
#endif
		break;
	case INSTR_HYPERCALL:
#ifndef __COMPCERT__
		*val = (uint32_t) save->rip + 3;
#else
		*val = save->rip_lo + 3;
#endif
		break;
	default:
		return 1;
	}

	return 0;
}

static bool
svm_pending_event(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_control_area *ctrl = &svm->vmcb->control;

	return (ctrl->event_inj & SVM_EVTINJ_VALID) ? TRUE : FALSE;
}

static bool
svm_intr_shadow(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_control_area *ctrl = &svm->vmcb->control;

	return (ctrl->int_state & 0x1) ? TRUE : FALSE;
}

#ifndef __COMPCERT__

struct hvm_ops hvm_ops_amd = {
	.signature		= AMD_SVM,
	.hw_init		= svm_init,
	.intercept_ioport	= svm_intercept_ioport,
	.intercept_msr		= svm_intercept_msr,
	.intercept_intr_window	= svm_intercept_vintr,
	.vm_init		= svm_init_vm,
	.vm_run			= svm_run_vm,
	.get_reg		= svm_get_reg,
	.set_reg		= svm_set_reg,
	.get_desc		= svm_get_desc,
	.set_desc		= svm_set_desc,
	.set_mmap		= svm_set_mmap,
	.inject_event		= svm_inject_event,
	.get_next_eip		= svm_get_next_eip,
	.pending_event		= svm_pending_event,
	.intr_shadow		= svm_intr_shadow,
};

#else

struct hvm_ops hvm_ops_amd;

void
init_hvm_ops_amd(void)
{
	hvm_ops_amd.signature		= AMD_SVM;
	hvm_ops_amd.hw_init		= svm_init;
	hvm_ops_amd.intercept_ioport	= svm_intercept_ioport;
	hvm_ops_amd.intercept_msr	= svm_intercept_msr;
	hvm_ops_amd.intercept_intr_window = svm_intercept_vintr;
	hvm_ops_amd.vm_init		= svm_init_vm;
	hvm_ops_amd.vm_run		= svm_run_vm;
	hvm_ops_amd.get_reg		= svm_get_reg;
	hvm_ops_amd.set_reg		= svm_set_reg;
	hvm_ops_amd.get_desc		= svm_get_desc;
	hvm_ops_amd.set_desc		= svm_set_desc;
	hvm_ops_amd.set_mmap		= svm_set_mmap;
	hvm_ops_amd.inject_event	= svm_inject_event;
	hvm_ops_amd.get_next_eip	= svm_get_next_eip;
	hvm_ops_amd.pending_event	= svm_pending_event;
	hvm_ops_amd.intr_shadow		= svm_intr_shadow;
}

#endif
