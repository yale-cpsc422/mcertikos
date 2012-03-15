#ifndef _VIRT_SVM_HANDLE_H_
#define _VIRT_SVM_HANDLE_H_

#ifdef _KERN_

#include <sys/trap.h>

#include <sys/virt/vmm.h>

void svm_inject_event(struct vmcb *,
		      uint32_t type, uint8_t vector, bool ev, uint32_t errcode);

void svm_guest_handle_gpf(struct vm *, tf_t *);
int svm_guest_intr_handler(struct vm *, uint8_t irq);

bool svm_handle_exception(struct vm *);
bool svm_handle_intr(struct vm *);
bool svm_handle_vintr(struct vm *);
bool svm_handle_ioio(struct vm *);
bool svm_handle_npf(struct vm *);
bool svm_handle_cpuid(struct vm *);
bool svm_handle_swint(struct vm *);
bool svm_handle_rdtsc(struct vm *);
bool svm_handle_err(struct vm *);

#endif /* _KERN_ */

#endif /* !_VIRT_SVM_HANDLE_H_ */
