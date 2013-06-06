#include <sys/debug.h>
#include <sys/mem.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/hvm.h>

#include <dev/intr.h>
#include <dev/pcpu.h>

#include "svm.h"
#include "svm_drv.h"

static struct svm svm0;

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

static exit_reason_t
svm_handle_exit(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);

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
		return EXIT_REASON_EXTINT;
	case SVM_EXIT_VINTR:
		ctrl->int_ctl &= ~SVM_INTR_CTRL_VIRQ;
		return EXIT_REASON_INTWIN;
	case SVM_EXIT_IOIO:
		svm->port = (exitinfo1 & SVM_EXITINFO1_PORT_MASK) >>
			SVM_EXITINFO1_PORT_SHIFT;
		svm->width = (exitinfo1 & SVM_EXITINFO1_SZ8) ? SZ8 :
			(exitinfo1 & SVM_EXITINFO1_SZ16) ? SZ16 : SZ32;
		svm->write = (exitinfo1 & SVM_EXITINFO1_TYPE_IN) ? FALSE : TRUE;
		svm->rep = (exitinfo1 & SVM_EXITINFO1_REP) ? TRUE : FALSE;
		svm->str = (exitinfo1 & SVM_EXITINFO1_STR) ? TRUE : FALSE;
		return EXIT_REASON_IOPORT;
	case SVM_EXIT_NPF:
		svm->fault_addr = (uintptr_t) PGADDR(exitinfo2);
		return EXIT_REASON_PGFLT;
	case SVM_EXIT_CPUID:
		return EXIT_REASON_CPUID;
	case SVM_EXIT_RDTSC:
		return EXIT_REASON_RDTSC;
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
		return EXIT_REASON_INVAL_INSTR;
	default:
		return EXIT_REASON_INVAL;
	}
}

/*
 * Initialize SVM module.
 * - check whether SVM is supported
 * - enable SVM
 * - allocate host save area
 *
 * @return 0 if succeed
 */
int
svm_init(void)
{
	memzero(&svm0, sizeof(svm0));
	svm0.inuse = 0;

	pageinfo_t *hsave_pi = mem_page_alloc();

	if (hsave_pi == NULL) {
		SVM_DEBUG("Cannot allocate memory for host state-save area.\n");
		return -1;
	}

	uintptr_t hsave_addr = mem_pi2phys(hsave_pi);
	SVM_DEBUG("Host state-save area is at %x.\n", hsave_addr);

	return svm_drv_init(hsave_addr);
}

struct svm *
svm_init_vm(void)
{
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

	return svm;

 err4:
	mem_pages_free(msrpm_pi);
 err3:
	mem_pages_free(iopm_pi);
 err2:
	mem_pages_free(vmcb_pi);
 err1:
	svm->inuse = 0;
 err0:
	return NULL;
}

exit_reason_t
svm_run_vm(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);

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
		return EXIT_REASON_INVAL;
	}

	return svm_handle_exit(svm);
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

void
svm_intercept_vintr(struct svm *svm, bool enable)
{
	KERN_ASSERT(svm != NULL);

	struct vmcb *vmcb = svm->vmcb;

	if (enable == TRUE) {
		svm_inject_vintr(vmcb, 0, 0);
		set_intercept(vmcb, INTERCEPT_VINTR, TRUE);
	} else {
		set_intercept(vmcb, INTERCEPT_VINTR, FALSE);
	}
}

uint32_t
svm_get_reg(struct svm *svm, guest_reg_t reg)
{
	KERN_ASSERT(svm != NULL);

	struct vmcb_save_area *save = &svm->vmcb->save;

	switch (reg) {
	case GUEST_EAX:
		return save->rax_lo;
	case GUEST_EBX:
		return (uint32_t) svm->g_rbx;
	case GUEST_ECX:
		return (uint32_t) svm->g_rcx;
	case GUEST_EDX:
		return (uint32_t) svm->g_rdx;
	case GUEST_ESI:
		return (uint32_t) svm->g_rsi;
	case GUEST_EDI:
		return (uint32_t) svm->g_rdi;
	case GUEST_EBP:
		return (uint32_t) svm->g_rbp;
	case GUEST_ESP:
		return save->rsp_lo;
	case GUEST_EIP:
		return save->rip_lo;
	case GUEST_EFLAGS:
		return save->rflags_lo;
	case GUEST_CR0:
		return save->cr0_lo;
	case GUEST_CR2:
		return save->cr2_lo;
	case GUEST_CR3:
		return save->cr3_lo;
	case GUEST_CR4:
		return save->cr4_lo;
	default:
		return 0xffffffff;
	}
}

int
svm_set_reg(struct svm *svm, guest_reg_t reg, uint32_t val)
{
	KERN_ASSERT(svm != NULL);

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
		return -1;
	}

	return 0;
}

int
svm_set_seg(struct svm *svm, guest_seg_t seg, uint16_t sel,
	    uint32_t base_lo, uint32_t base_hi, uint32_t lim, uint32_t ar)
{
	KERN_ASSERT(svm != NULL);

	struct vmcb_save_area *save = &svm->vmcb->save;
	struct vmcb_seg *vmcb_seg;

	switch (seg) {
	case GUEST_CS:
		vmcb_seg = &save->cs;
		break;
	case GUEST_DS:
		vmcb_seg = &save->ds;
		break;
	case GUEST_ES:
		vmcb_seg = &save->es;
		break;
	case GUEST_FS:
		vmcb_seg = &save->fs;
		break;
	case GUEST_GS:
		vmcb_seg = &save->gs;
		break;
	case GUEST_SS:
		vmcb_seg = &save->ss;
		break;
	case GUEST_LDTR:
		vmcb_seg = &save->ldtr;
		break;
	case GUEST_TR:
		vmcb_seg = &save->tr;
		break;
	case GUEST_GDTR:
		vmcb_seg = &save->gdtr;
		break;
	case GUEST_IDTR:
		vmcb_seg = &save->idtr;
		break;
	default:
		return -1;
	}

	vmcb_seg->selector = sel;
	vmcb_seg->base_lo = base_lo;
	vmcb_seg->base_hi = base_hi;
	vmcb_seg->limit = lim;
	vmcb_seg->attrib = (ar & 0xff) | ((ar & 0xf000) >> 4);

	return 0;
}

int
svm_set_mmap(struct svm *svm, uintptr_t gpa, uintptr_t hpa)
{
	KERN_ASSERT(svm != NULL);

	if (ROUNDDOWN(gpa, PAGESIZE) != gpa || ROUNDDOWN(hpa, PAGESIZE) != hpa)
		return 1;

	struct vmcb_control_area *ctrl = &svm->vmcb->control;
	pmap_t *npt = (pmap_t *)(uintptr_t) ctrl->nested_cr3_lo;

	npt = pmap_insert(npt, mem_phys2pi(hpa), gpa, PTE_W | PTE_G | PTE_U);

	return (npt == NULL) ? 2 : 0;
}

int
svm_inject_event(struct svm *svm,
		 guest_event_t type, uint8_t vector, uint32_t errcode, bool ev)
{
	KERN_ASSERT(svm != NULL);

	int evt_type;

	if (type == EVENT_EXTINT)
		evt_type = SVM_EVTINJ_TYPE_INTR;
	else if (type == EVENT_NMI)
		evt_type = SVM_EVTINJ_TYPE_NMI;
	else if (type == EVENT_EXCEPTION)
		evt_type = SVM_EVTINJ_TYPE_EXEPT;
	else if (type == EVENT_SWINT)
		evt_type = SVM_EVTINJ_TYPE_SOFT;
	else
		return 1;

	struct vmcb_control_area *ctrl = &svm->vmcb->control;

	if (ctrl->event_inj & SVM_EVTINJ_VALID)
		return 2;

	ctrl->event_inj = SVM_EVTINJ_VALID | (vector & SVM_EVTINJ_VEC_MASK) |
		((evt_type << SVM_EVTINJ_TYPE_SHIFT) & SVM_EVTINJ_TYPE_MASK);
	ctrl->event_inj_err = errcode;

	if (ev == TRUE)
		ctrl->event_inj |= SVM_EVTINJ_VALID_ERR;

	return 0;
}

uint32_t
svm_get_next_eip(struct svm *svm, guest_instr_t instr)
{
	KERN_ASSERT(svm != NULL);

	struct vmcb_control_area *ctrl = &svm->vmcb->control;

	switch (instr) {
	case INSTR_IN:
	case INSTR_OUT:
		return ctrl->exit_info_2_lo;
	default:
		return ctrl->next_rip_lo;
	}
}

bool
svm_pending_event(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);
	struct vmcb_control_area *ctrl = &svm->vmcb->control;
	return (ctrl->event_inj & SVM_EVTINJ_VALID) ? TRUE : FALSE;
}

bool
svm_intr_shadow(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);
	struct vmcb_control_area *ctrl = &svm->vmcb->control;
	return (ctrl->int_state & 0x1) ? TRUE : FALSE;
}

uint16_t
svm_exit_io_port(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);
	return svm->port;
}

data_sz_t
svm_exit_io_width(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);
	return svm->width;
}

bool
svm_exit_io_write(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);
	return svm->write;
}

bool
svm_exit_io_rep(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);
	return svm->rep;
}

bool
svm_exit_io_str(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);
	return svm->str;
}

uintptr_t
svm_exit_fault_addr(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);
	return svm->fault_addr;
}
