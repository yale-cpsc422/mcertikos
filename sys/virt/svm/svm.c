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

static void init_seg_offset(void);
static void init_svm_event_type(void);

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
}

static int
svm_handle_exit(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_control_area *ctrl = &vmcb->control;
	uint32_t exitinfo1 = ctrl->exit_info_1_lo;
	uint32_t exitinfo2 = ctrl->exit_info_2_lo;

#ifdef DEBUG_VMEXIT
	SVM_DEBUG("VMEXIT exit_code 0x%x, guest EIP 0x%08x.\n",
		  ctrl->exit_code, vmcb->save.rip_lo);
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
		vm->exit_info.pgflt.addr = (uintptr_t) PGADDR(exitinfo2);
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

	init_seg_offset();
	init_svm_event_type();

	pageinfo_t *hsave_pi = mem_page_alloc();

	if (hsave_pi == NULL) {
		SVM_DEBUG("Cannot allocate memory for host state-save area.\n");
		return -1;
	}

	uintptr_t hsave_addr = mem_pi2phys(hsave_pi);
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
	svm->vmcb->control.iopm_base_pa_lo = mem_pi2phys(iopm_pi);
	svm->vmcb->control.iopm_base_pa_hi = 0;

	if ((msrpm_pi = mem_pages_alloc(SVM_MSRPM_SIZE)) == NULL) {
		rc = -4;
		goto err3;
	}
	svm->vmcb->control.msrpm_base_pa_lo = mem_pi2phys(msrpm_pi);
	svm->vmcb->control.msrpm_base_pa_hi = 0;

	if ((ncr3_pi = mem_page_alloc()) == NULL) {
		rc = -5;
		goto err4;
	}
	svm->vmcb->control.nested_cr3_lo = mem_pi2phys(ncr3_pi);
	svm->vmcb->control.nested_cr3_hi = 0;
	memzero((void *) svm->vmcb->control.nested_cr3_lo, PAGESIZE);

	/*
	 * Setup default interception.
	 */

	/* intercept all I/O ports */
	memset((void *) svm->vmcb->control.iopm_base_pa_lo,
	       0xf, SVM_IOPM_SIZE);
	/* do not intercept any MSR */
	memzero((void *) svm->vmcb->control.msrpm_base_pa_lo,
		SVM_MSRPM_SIZE);

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

	svm->vmcb->save.dr6_lo = 0xffff0ff0;
	svm->vmcb->save.dr6_hi = 0;
	svm->vmcb->save.dr7_lo = 0x00000400;
	svm->vmcb->save.dr7_hi = 0;
	svm->vmcb->save.efer_lo = MSR_EFER_SVME;
	svm->vmcb->save.efer_hi = 0;
	svm->vmcb->save.g_pat_lo = 0x00070406;
	svm->vmcb->save.g_pat_hi = 0x00070406;

	svm->vmcb->control.asid = 1;
	svm->vmcb->control.nested_ctl_lo = 1;
	svm->vmcb->control.nested_ctl_hi = 0;
	svm->vmcb->control.tlb_ctl = 0;
	svm->vmcb->control.tsc_offset_lo = 0;
	svm->vmcb->control.tsc_offset_hi = 0;
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
		*val = save->rax_lo;
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
		*val = save->rsp_lo;
		break;
	case GUEST_EIP:
		*val = save->rip_lo;
		break;
	case GUEST_EFLAGS:
		*val = save->rflags_lo;
		break;
	case GUEST_CR0:
		*val = save->cr0_lo;
		break;
	case GUEST_CR2:
		*val = save->cr2_lo;
		break;
	case GUEST_CR3:
		*val = save->cr3_lo;
		break;
	case GUEST_CR4:
		*val = save->cr4_lo;
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
		save->rax_lo = val;
		save->rax_hi = 0;
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
		save->rsp_lo = val;
		save->rsp_hi = 0;
		break;
	case GUEST_EIP:
		save->rip_lo = val;
		save->rip_hi = 0;
		break;
	case GUEST_EFLAGS:
		save->rflags_lo = val;
		save->rflags_hi = 0;
		break;
	case GUEST_CR0:
		save->cr0_lo = val;
		save->cr0_hi = 0;
		break;
	case GUEST_CR2:
		save->cr2_lo = val;
		save->cr2_hi = 0;
		break;
	case GUEST_CR3:
		save->cr3_lo = val;
		save->cr3_hi = 0;
		break;
	case GUEST_CR4:
		save->cr4_lo = val;
		save->cr4_hi = 0;
		break;
	default:
		return 1;
	}

	return 0;
}

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
	desc->base_lo = vmcb_seg->base_lo;
	desc->base_hi = vmcb_seg->base_hi;
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
	vmcb_seg->base_lo = desc->base_lo;
	vmcb_seg->base_hi = desc->base_hi;
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
	pmap_t *npt = (pmap_t *)(uintptr_t) ctrl->nested_cr3_lo;

	npt = pmap_insert(npt, mem_phys2pi(hpa), gpa, PTE_W | PTE_G | PTE_U);

	return (npt == NULL) ? 2 : 0;
}

static int svm_event_type[4];

static void
init_svm_event_type(void)
{
	svm_event_type[EVENT_EXTINT]	= SVM_EVTINJ_TYPE_INTR;
	svm_event_type[EVENT_NMI]	= SVM_EVTINJ_TYPE_NMI;
	svm_event_type[EVENT_EXCEPTION]	= SVM_EVTINJ_TYPE_EXEPT;
	svm_event_type[EVENT_SWINT]	= SVM_EVTINJ_TYPE_SOFT;
}

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
		*val = ctrl->exit_info_2_lo;
		break;
	case INSTR_RDMSR:
	case INSTR_WRMSR:
	case INSTR_CPUID:
	case INSTR_RDTSC:
		*val = save->rip_lo + 2;
		break;
	case INSTR_HYPERCALL:
		*val = save->rip_lo + 3;
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

struct hvm_ops hvm_ops_amd;

void
init_hvm_ops_amd(void)
{
	hvm_ops_amd.signature		= AMD_SVM;
	hvm_ops_amd.hw_init		= svm_init;
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
