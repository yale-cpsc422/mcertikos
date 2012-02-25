#ifndef _HVM_SVM_HANDLE_H_
#define _HVM_SVM_HANDLE_H_

#include <architecture/context_internal.h>

#include <kern/hvm/vmm.h>

void svm_guest_handle_gpf(struct vm *, trapframe *);
void svm_guest_intr_handler(struct vm *, trapframe *);

bool svm_handle_exception(struct vm *);
bool svm_handle_intr(struct vm *);
bool svm_handle_vintr(struct vm *);
bool svm_handle_swint(struct vm *);
bool svm_handle_ioio(struct vm *);
bool svm_handle_npf(struct vm *);
bool svm_handle_cpuid(struct vm *);
bool svm_handle_rdtsc(struct vm *);
bool svm_handle_err(struct vm *);

#endif /* !_HVM_SVM_HANDLE_H_ */
