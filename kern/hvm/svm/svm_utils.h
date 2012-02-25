#ifndef _HVM_SVM_UTILS_H_
#define _HVM_SVM_UTILS_H_

#include <architecture/types.h>

#include <kern/hvm/svm/svm.h>

uintptr_t glogic_2_glinear(struct vmcb *, uint16_t seg_select, uint32_t offset);
uintptr_t glinear_2_gphysical(struct vmcb *, uintptr_t la);
uint8_t *get_guest_instruction(struct vmcb *vmcb);
void load_bios(uintptr_t ncr3);

#endif /* !_HVM_SVM_UTILS_H_ */
