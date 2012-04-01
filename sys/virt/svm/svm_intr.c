#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>

#include "svm.h"
#include "svm_handle.h"


void detect_guest_intr(struct vm *vm)
{
	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;

	/* If a EOI has not been received for a pending
	 * intr, then just return
	 *
	
	if (svm->intr_in_waiting)
	return;*/

	/* 
	 * Enable VINTR so that we can VMEXIT 
	 * when guest is ready to accept interrupts
	 */
	set_intercept(vmcb, INTERCEPT_VINTR, TRUE);
	
	/* Inject a fake interrupt */
	svm_inject_vintr(vmcb, 0x0, 0xff);
	
}


void guest_intr_inject_event(struct vm *vm)
{
	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_control_area *ctrl  = &vmcb->control;

	int irq = vpic_read_irq(&vm->vpic);

	/* Clear up vintr so we don't get another one
	 * while we are handling this
	 */
	ctrl->int_ctl = ~SVM_INTR_CTRL_VIRQ & ~((0xFF << 16) & SVM_INTR_CTRL_PRIO) 
		& ~SVM_INTR_CTRL_IGN_VTPR;
	set_intercept(vmcb, INTERCEPT_VINTR, FALSE);

	if (irq == -1) {
		KERN_DEBUG("No pending IRQ found \n");
		return;
	}

	svm_inject_event(vmcb, SVM_EVTINJ_TYPE_INTR, 9, FALSE, 0);
}

