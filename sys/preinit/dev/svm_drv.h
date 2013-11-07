#ifndef _SYS_PREINIT_DEV_SVM_DRV_H_
#define _SYS_PREINIT_DEV_SVM_DRV_H_

#ifdef _KERN_

#include <lib/types.h>

int svm_drv_init(void);

void enter_guest(void *vmcb,
		 uint32_t *g_ebx, uint32_t *g_ecx, uint32_t *g_edx,
		 uint32_t *g_esi, uint32_t *g_edi, uint32_t *g_ebp,
		 uint32_t *h_ebx, uint32_t *h_ecx, uint32_t *h_edx,
		 uint32_t *h_esi, uint32_t *h_edi, uint32_t *h_ebp);

#endif /* _KERN_ */

#endif /* !_SYS_PREINIT_DEV_SVM_DRV_H_ */