#ifndef _VIRT_SVM_INTR_H
#define _VIRT_SVM_INTR_H

void detect_guest_intr(struct vm *vm);
void guest_intr_inject_event(struct vm *vm);

#endif
