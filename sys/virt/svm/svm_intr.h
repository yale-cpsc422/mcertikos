#ifndef _VIRT_SVM_INTR_H_
#define _VIRT_SVM_INTR_H_

#ifdef _KERN_

#include <sys/virt/vmm.h>

void svm_intr_assist(struct vm *);

#endif /* _KERN_ */

#endif /* !_VIRT_SVM_INTR_H_ */
