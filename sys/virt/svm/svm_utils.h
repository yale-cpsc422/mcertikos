#ifndef _VIRT_SVM_UTILS_H_
#define _VIRT_SVM_UTILS_H_

#ifdef _KERN_

#include <sys/types.h>

#include "svm.h"

uintptr_t glogic_2_glinear(struct vmcb *, uint16_t seg_select, uint32_t offset);
uintptr_t glinear_2_gphysical(struct vmcb *, uintptr_t la);
uint8_t *get_guest_instruction(struct vmcb *vmcb);

#endif /* _KERN_ */

#endif /* !_VIRT_SVM_UTILS_H_ */
