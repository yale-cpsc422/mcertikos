#include <lib/export.h>

#include "vmcb.h"

struct vmcb vmcb0 gcc_aligned(4096);
static uint8_t iopm0[SVM_IOPM_SIZE] gcc_aligned(4096);
static bool vmcb0_inited = FALSE;

void
vmcb_init(void)
{
	if (vmcb0_inited == TRUE)
		return;

	struct vmcb *vmcb = &vmcb0;

	memzero(vmcb, sizeof(struct vmcb));

	vmcb->save.dr6_lo = 0xffff0ff0;
	vmcb->save.dr6_hi = 0;
	vmcb->save.dr7_lo = 0x00000400;
	vmcb->save.dr7_hi = 0;
	vmcb->save.efer_lo = (1 << 12);	/* MSR_EFER_SVME */
	vmcb->save.efer_hi = 0;
	vmcb->save.g_pat_lo = 0x00070406;
	vmcb->save.g_pat_hi = 0x00070406;

	vmcb->control.asid = 1;
	vmcb->control.nested_ctl_lo = 1;
	vmcb->control.nested_ctl_hi = 0;
	vmcb->control.tlb_ctl = 0;
	vmcb->control.tsc_offset_lo = 0;
	vmcb->control.tsc_offset_hi = 0;
	/* Sec 15.21.1, APM Vol2 r3.19 */
	vmcb->control.int_ctl =
		(SVM_INTR_CTRL_VINTR_MASK | (0x0 & SVM_INTR_CTRL_VTPR));

	memset(iopm0, 0xff, SVM_IOPM_SIZE);
	vmcb->control.iopm_base_pa_lo = (uintptr_t) iopm0;
	vmcb->control.iopm_base_pa_hi = 0;

	/* setup default interception */
	vmcb->control.intercept_lo = (1UL << INTERCEPT_INTR) |
		(1UL << INTERCEPT_RDTSC) | (1UL << INTERCEPT_CPUID) |
		(1UL << INTERCEPT_HLT) | (1UL << INTERCEPT_IOIO_PROT);
	vmcb->control.intercept_hi = (1UL << (INTERCEPT_VMRUN - 32)) |
		(1UL << (INTERCEPT_VMMCALL - 32)) |
		(1UL << (INTERCEPT_VMLOAD - 32)) |
		(1UL << (INTERCEPT_VMSAVE - 32)) |
		(1UL << (INTERCEPT_STGI - 32)) |
		(1UL << (INTERCEPT_CLGI - 32)) |
		(1UL << (INTERCEPT_SKINIT - 32)) |
		(1UL << (INTERCEPT_RDTSCP - 32)) |
		(1UL << (INTERCEPT_MONITOR - 32)) |
		(1UL << (INTERCEPT_MWAIT - 32)) |
		(1UL << (INTERCEPT_MWAIT_COND - 32));

	vmcb0_inited = TRUE;
}

void
vmcb_set_intercept_vint(void)
{
	vmcb0.control.intercept_lo |= (1UL << INTERCEPT_VINTR);
}

void
vmcb_clear_intercept_vint(void)
{
	vmcb0.control.intercept_lo &= ~(1UL << INTERCEPT_VINTR);
}

void
vmcb_clear_virq(void)
{
	vmcb0.control.int_ctl &= ~SVM_INTR_CTRL_VIRQ;
}

void
vmcb_inject_virq(void)
{
	struct vmcb_control_area *ctrl = &vmcb0.control;

	ctrl->int_ctl |= SVM_INTR_CTRL_VIRQ |
		((0 << SVM_INTR_CTRL_PRIO_SHIFT) & SVM_INTR_CTRL_PRIO) |
		SVM_INTR_CTRL_IGN_VTPR;
	ctrl->int_vector = 0;
}

uint32_t
vmcb_get_exit_code(void)
{
	return vmcb0.control.exit_code;
}

uint32_t
vmcb_get_exit_info1(void)
{
	return vmcb0.control.exit_info_1_lo;
}

uint32_t
vmcb_get_exit_info2(void)
{
	return vmcb0.control.exit_info_2_lo;
}

uint32_t
vmcb_get_exit_intinfo(void)
{
	return vmcb0.control.exit_int_info;
}

uint32_t
vmcb_get_exit_interr(void)
{
	return vmcb0.control.exit_int_info_err;
}

bool
vmcb_check_int_shadow(void)
{
	return (vmcb0.control.int_state & 0x1) ? TRUE : FALSE;
}

void
vmcb_inject_event(int type, uint8_t vector, uint32_t errcode, bool ev)
{
	KERN_ASSERT(type == SVM_EVTINJ_TYPE_INTR ||
		    type == SVM_EVTINJ_TYPE_NMI ||
		    type == SVM_EVTINJ_TYPE_EXEPT ||
		    type == SVM_EVTINJ_TYPE_SOFT);

	struct vmcb_control_area *ctrl = &vmcb0.control;

	KERN_ASSERT((ctrl->event_inj & SVM_EVTINJ_VALID) == 0);

	ctrl->event_inj = SVM_EVTINJ_VALID | (vector & SVM_EVTINJ_VEC_MASK) |
		((type << SVM_EVTINJ_TYPE_SHIFT) & SVM_EVTINJ_TYPE_MASK);
	ctrl->event_inj_err = errcode;

	if (ev == TRUE)
		ctrl->event_inj |= SVM_EVTINJ_VALID_ERR;
}

bool
vmcb_check_pending_event(void)
{
	return (vmcb0.control.event_inj & SVM_EVTINJ_VALID) ? TRUE : FALSE;
}

uint32_t
vmcb_get_next_eip(void)
{
	return vmcb0.control.next_rip_lo;
}

void
vmcb_set_seg(guest_seg_t seg,
	     uint16_t sel, uint32_t base, uint32_t lim, uint32_t ar)
{
	struct vmcb_save_area *save = &vmcb0.save;
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
		KERN_PANIC("Unrecognized segment %d.\n", seg);
		vmcb_seg = NULL;
	}

	vmcb_seg->selector = sel;
	vmcb_seg->base_lo = base;
	vmcb_seg->base_hi = 0;
	vmcb_seg->limit = lim;
	vmcb_seg->attrib = (ar & 0xff) | ((ar & 0xf000) >> 4);
}

void
vmcb_set_reg(guest_reg_t reg, uint32_t val)
{
	struct vmcb_save_area *save = &vmcb0.save;

	switch (reg) {
	case GUEST_EAX:
		save->rax_lo = val;
		save->rax_hi = 0;
		break;
	case GUEST_EIP:
		save->rip_lo = val;
		save->rip_hi = 0;
		break;
	case GUEST_ESP:
		save->rsp_lo = val;
		save->rsp_hi = 0;
		break;
	case GUEST_EFLAGS:
		save->rflags_lo = val;
		save->rflags_hi = val;
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
		KERN_PANIC("Unrecognized register %d.\n", reg);
	}
}

uint32_t
vmcb_get_reg(guest_reg_t reg)
{
	struct vmcb_save_area *save = &vmcb0.save;

	switch (reg) {
	case GUEST_EAX:
		return save->rax_lo;
	case GUEST_EIP:
		return save->rip_lo;
	case GUEST_ESP:
		return save->rsp_lo;
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
		KERN_PANIC("Unrecognized register %d.\n", reg);
		return 0xffffffff;
	}
}
