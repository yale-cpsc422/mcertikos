#include <lib/export.h>

#include "vmcb.h"

static struct vmcb vmcb0 gcc_aligned(4096);
static uint8_t iopm0[SVM_IOPM_SIZE] gcc_aligned(4096);
static int vmcb0_inuse = 0;

struct vmcb *
vmcb_new(void)
{
	if (vmcb0_inuse == 1)
		return NULL;

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

	vmcb0_inuse = 1;

	return vmcb;
}

void
vmcb_free(struct vmcb *vmcb)
{
	if (vmcb == &vmcb0)
		return;
	vmcb0_inuse = 0;
}

int
vmcb_set_intercept(struct vmcb *vmcb, int bit)
{
	if ((uintptr_t) vmcb % 4096)
		return -1;

	if (!(INTERCEPT_INTR <= bit && bit <= INTERCEPT_XSETBV))
		return -2;

	if (bit < 32)
		vmcb->control.intercept_lo |= (1UL << bit);
	else
		vmcb->control.intercept_hi |= (1UL << (bit - 32));

	return 0;
}

int
vmcb_clear_intercept(struct vmcb *vmcb, int bit)
{
	if ((uintptr_t) vmcb % 4096)
		return -1;

	if (!(INTERCEPT_INTR <= bit && bit <= INTERCEPT_XSETBV))
		return -2;

	if (bit < 32)
		vmcb->control.intercept_lo &= ~(1UL << bit);
	else
		vmcb->control.intercept_hi &= ~(1UL << (bit - 32));

	return 0;
}

int
vmcb_clear_virq(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return -1;
	vmcb->control.int_ctl &= ~SVM_INTR_CTRL_VIRQ;
	return 0;
}

int
vmcb_inject_virq(struct vmcb *vmcb, uint8_t vector, uint8_t priority)
{
	if ((uintptr_t) vmcb % 4096)
		return -1;

	struct vmcb_control_area *ctrl = &vmcb->control;

	ctrl->int_ctl |= SVM_INTR_CTRL_VIRQ |
		((priority << SVM_INTR_CTRL_PRIO_SHIFT) & SVM_INTR_CTRL_PRIO) |
		SVM_INTR_CTRL_IGN_VTPR;
	ctrl->int_vector = vector;

	return 0;
}

uint32_t
vmcb_get_exit_code(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return 0xffffffff;
	return vmcb->control.exit_code;
}

uint32_t
vmcb_get_exit_info1(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return 0xffffffff;
	return vmcb->control.exit_info_1_lo;
}

uint32_t
vmcb_get_exit_info2(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return 0xffffffff;
	return vmcb->control.exit_info_2_lo;
}

uint32_t
vmcb_get_exit_int_info(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return 0xffffffff;
	return vmcb->control.exit_int_info;
}

uint32_t
vmcb_get_exit_int_errcode(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return 0xffffffff;
	return vmcb->control.exit_int_info_err;
}

bool
vmcb_get_intr_shadow(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return FALSE;
	return (vmcb->control.int_state & 0x1) ? TRUE : FALSE;
}

int
vmcb_inject_event(struct vmcb *vmcb, int type, uint8_t vector,
		  uint32_t errcode, bool ev)
{
	if ((uintptr_t) vmcb % 4096)
		return -1;

	if (type != SVM_EVTINJ_TYPE_INTR && type != SVM_EVTINJ_TYPE_NMI &&
	    type != SVM_EVTINJ_TYPE_EXEPT && type != SVM_EVTINJ_TYPE_SOFT)
		return -2;

	struct vmcb_control_area *ctrl = &vmcb->control;

	if (ctrl->event_inj & SVM_EVTINJ_VALID)
		return -3;

	ctrl->event_inj = SVM_EVTINJ_VALID | (vector & SVM_EVTINJ_VEC_MASK) |
		((type << SVM_EVTINJ_TYPE_SHIFT) & SVM_EVTINJ_TYPE_MASK);
	ctrl->event_inj_err = errcode;

	if (ev == TRUE)
		ctrl->event_inj |= SVM_EVTINJ_VALID_ERR;

	return 0;
}

bool
vmcb_pending_event(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return FALSE;
	return (vmcb->control.event_inj & SVM_EVTINJ_VALID) ? TRUE : FALSE;
}

int
vmcb_set_ncr3(struct vmcb *vmcb, uintptr_t ncr3)
{
	if ((uintptr_t) vmcb % 4096)
		return -1;

	if (ncr3 % 4096)
		return -2;

	vmcb->control.nested_cr3_lo = ncr3;
	vmcb->control.nested_cr3_hi = 0;

	return 0;
}

uint32_t
vmcb_get_neip(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return 0xffffffff;
	return vmcb->control.next_rip_lo;
}

int
vmcb_set_seg(struct vmcb *vmcb, guest_seg_t seg,
	     uint16_t sel, uint32_t base, uint32_t lim, uint32_t ar)
{
	if ((uintptr_t) vmcb % 4096)
		return -1;

	struct vmcb_save_area *save = &vmcb->save;
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
		return -2;
	}

	vmcb_seg->selector = sel;
	vmcb_seg->base_lo = base;
	vmcb_seg->base_hi = 0;
	vmcb_seg->limit = lim;
	vmcb_seg->attrib = (ar & 0xff) | ((ar & 0xf000) >> 4);

	return 0;
}

uint32_t
vmcb_get_cr0(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return 0xffffffff;
	return vmcb->save.cr0_lo;
}

uint32_t
vmcb_get_cr2(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return 0xffffffff;
	return vmcb->save.cr2_lo;
}

uint32_t
vmcb_get_cr3(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return 0xffffffff;
	return vmcb->save.cr3_lo;
}

uint32_t
vmcb_get_cr4(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return 0xffffffff;
	return vmcb->save.cr4_lo;
}

int
vmcb_set_cr0(struct vmcb *vmcb, uint32_t val)
{
	if ((uintptr_t) vmcb % 4096)
		return -1;

	vmcb->save.cr0_lo = val;
	vmcb->save.cr0_hi = 0;

	return 0;
}

int
vmcb_set_cr2(struct vmcb *vmcb, uint32_t val)
{
	if ((uintptr_t) vmcb % 4096)
		return -1;

	vmcb->save.cr2_lo = val;
	vmcb->save.cr2_hi = 0;

	return 0;
}

int
vmcb_set_cr3(struct vmcb *vmcb, uint32_t val)
{
	if ((uintptr_t) vmcb % 4096)
		return -1;

	vmcb->save.cr3_lo = val;
	vmcb->save.cr3_hi = 0;

	return 0;
}

int
vmcb_set_cr4(struct vmcb *vmcb, uint32_t val)
{
	if ((uintptr_t) vmcb % 4096)
		return -1;

	vmcb->save.cr4_lo = val;
	vmcb->save.cr4_hi = 0;

	return 0;
}

uint32_t
vmcb_get_eip(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return 0xffffffff;
	return vmcb->save.rip_lo;
}

int
vmcb_set_eip(struct vmcb *vmcb, uint32_t eip)
{
	if ((uintptr_t) vmcb % 4096)
		return -1;

	vmcb->save.rip_lo = eip;
	vmcb->save.rip_hi = 0;

	return 0;
}

uint32_t
vmcb_get_esp(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return 0xffffffff;
	return vmcb->save.rsp_lo;
}

int
vmcb_set_esp(struct vmcb *vmcb, uint32_t esp)
{
	if ((uintptr_t) vmcb % 4096)
		return -1;

	vmcb->save.rsp_lo = esp;
	vmcb->save.rsp_hi = 0;

	return 0;
}

uint32_t
vmcb_get_eax(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return 0xffffffff;
	return vmcb->save.rax_lo;
}

int
vmcb_set_eax(struct vmcb *vmcb, uint32_t eax)
{
	if ((uintptr_t) vmcb % 4096)
		return -1;

	vmcb->save.rax_lo = eax;
	vmcb->save.rax_hi = 0;

	return 0;
}

uint32_t
vmcb_get_eflags(struct vmcb *vmcb)
{
	if ((uintptr_t) vmcb % 4096)
		return 0xffffffff;
	return vmcb->save.rflags_lo;
}

int
vmcb_set_eflags(struct vmcb *vmcb, uint32_t eflags)
{
	if ((uintptr_t) vmcb % 4096)
		return -1;

	vmcb->save.rflags_lo = eflags;
	vmcb->save.rflags_hi = 0;

	return 0;
}
