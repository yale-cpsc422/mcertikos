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
#ifdef DEBUG_GUEST_INTR
		KERN_DEBUG("INTR is blocked due to interrpts being disabled.\n");
#endif
		return TRUE;
	}

	if (ctrl->int_state & 0x1) {
#ifdef DEBUG_GUEST_INTR
		KERN_DEBUG("INTR is blocked due to interrupt shadow.\n");
#endif
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
	struct vmcb_save_area *save = &vmcb->save;

	struct vpic *pic = &vm->vpic;
	int intr_vec;

	/* no interrupt pending */
	if ((intr_vec = vpic_get_irq(pic)) == -1)
		return;

	if (ctrl->event_inj & SVM_EVTINJ_VALID ||
	    svm_intr_blocked(vmcb) == TRUE) {
		/* XXX: is the priority correct? */
		svm_inject_vintr(vmcb, 0, intr_vec >> 4);
		svm->pending_vintr = intr_vec;
		set_intercept(vmcb, INTERCEPT_VINTR, TRUE);

		/* check pending HLT */
		if (vm->halt_for_hlt == TRUE) {
#ifdef DEBUG_GUEST_HLT
			KERN_DEBUG("Clear HLT flag.\n");
#endif
			KERN_ASSERT(save->rip == vm->hlt_rip);

			save->rip += 1;
			vm->halt_for_hlt = FALSE;
		}

		return;
	}

	intr_vec = vpic_read_irq(pic);
	svm_inject_event(vmcb, SVM_EVTINJ_TYPE_INTR, intr_vec, FALSE, 0);
	svm->pending_vintr = -1;
	set_intercept(vmcb, INTERCEPT_VINTR, FALSE);

	/* check pending HLT */
	if (vm->halt_for_hlt == TRUE) {
#ifdef DEBUG_GUEST_HLT
		KERN_DEBUG("Clear HLT flag.\n");
#endif
		KERN_ASSERT(save->rip == vm->hlt_rip);

		save->rip += 1;
		vm->halt_for_hlt = FALSE;
	}
}
