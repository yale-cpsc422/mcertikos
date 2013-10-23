#ifndef _KERN_VIRT_SVM_SWITCH_H_
#define _KERN_VIRT_SVM_SWITCH_H_

#ifdef _KERN_

void svm_switch(struct vmcb *vmcb,
		uint32_t *ebx, uint32_t *ecx, uint32_t *edx,
		uint32_t *esi, uint32_t *edi, uint32_t *ebp);

#endif /* _KERN_ */

#endif /* !_KERN_VIRT_SVM_SWITCH_H_ */
