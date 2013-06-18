#ifndef _KERN_SVM_DRV_H_
#define _KERN_SVM_DRV_H_

#include <sys/types.h>
#include <sys/virt/hvm.h>
#include "svm.h"

int svm_drv_init(void);
void svm_drv_run_vm(struct vmcb *vmcb, uint32_t *ebx, uint32_t *ecx,
		    uint32_t *edx, uint32_t *esi, uint32_t *edi, uint32_t *ebp);

#ifdef DEBUG_MSG
int svm_handle_err(struct vmcb *vm);
#endif

#endif /* !_KERN_SVM_DRV_H_ */
