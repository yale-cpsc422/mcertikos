#ifndef _KERN_DEV_SVM_DRV_H_
#define _KERN_DEV_SVM_DRV_H_

#ifdef _KERN_

#include <lib/types.h>

#include "vmcb.h"

int svm_drv_init(void);
void enter_guest(uint32_t *ebx, uint32_t *ecx,
		 uint32_t *edx, uint32_t *esi, uint32_t *edi, uint32_t *ebp);

#endif /* _KERN_ */

#endif /* !_KERN_DEV_SVM_DRV_H_ */
