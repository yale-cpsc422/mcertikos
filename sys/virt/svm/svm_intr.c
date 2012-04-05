#include <sys/debug.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/dev/pic.h>

#include "svm.h"
#include "svm_handle.h"

/*
 * Check whether the processor in the guest can response to external interrupts.
 * The interrupts can be block due to:
 * 1. IF bit of EFLAGS of the guest is cleared, or
 * 2. The guest is in the interrupt shadow.
 *
 * @return TRUE if the guest can not response the external interrupts;
 *         otherwise, FALSE.
 */
static bool
svm_intr_blocked(struct vmcb *vmcb)
{
	KERN_ASSERT(vmcb != NULL);

	struct vmcb_control_area *ctrl = &vmcb->control;
	struct vmcb_save_area *save = &vmcb->save;

	if (!(save->rflags & FL_IF)) {
		KERN_DEBUG("INTR is blocked due to interrpts being disabled.\n");
		return TRUE;
	}

	if (ctrl->int_state & 0x1) {
		KERN_DEBUG("INTR is blocked due to interrupt shadow.\n");
		return TRUE;
	}

	return FALSE;
}

/*
 * Inject pending interrupts to the guest according to the interrupt setup of
 * the guest.
 */
void
svm_intr_assist(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_control_area *ctrl = &vmcb->control;

	struct vpic *pic = &vm->vpic;
	int intr_vec;

	/* no interrupt pending */
	if ((intr_vec = vpic_get_irq(pic)) == -1) {
		/* KERN_ASSERT(!(ctrl->int_ctl & SVM_INTR_CTRL_VIRQ)); */
		return;
	}

	/* virtual interrupt pending or interrupts in guest is blocked */
	if (ctrl->int_ctl & SVM_INTR_CTRL_VIRQ &&
	    svm->pending_vintr == intr_vec) {
		KERN_DEBUG("INTR vec=%x is already pending.\n", intr_vec);
		return;
	}

	/* KERN_DEBUG("Found pending INTR: vec=%x.\n", intr_vec); */

	if (ctrl->int_ctl & SVM_INTR_CTRL_VIRQ ||
	    svm_intr_blocked(vmcb) == TRUE) {
		/* XXX: is the priority correct? */
		svm_inject_vintr(vmcb, 0, intr_vec >> 4);
		svm->pending_vintr = intr_vec;
		set_intercept(vmcb, INTERCEPT_VINTR, TRUE);
		return;
	}

	intr_vec = vpic_read_irq(pic);
	svm_inject_event(vmcb, SVM_EVTINJ_TYPE_INTR, intr_vec, FALSE, 0);
	svm->pending_vintr = -1;
	set_intercept(vmcb, INTERCEPT_VINTR, FALSE);
}
