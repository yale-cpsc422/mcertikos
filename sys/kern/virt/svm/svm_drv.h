#ifndef _KERN_SVM_DRV_H_
#define _KERN_SVM_DRV_H_

#include <sys/types.h>
#include <sys/virt/hvm.h>
#include "svm.h"

int svm_drv_init(uintptr_t hsave_addr);
void svm_drv_run_vm(struct svm *svm);
int svm_handle_err(struct vmcb *vm);

#endif /* !_KERN_SVM_DRV_H_ */
