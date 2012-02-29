#ifndef _KERN_INTR_H_
#define _KERN_INTR_H_

#ifndef _KERN_
#error "This is a kernel header file; do not include it in userspace program."
#endif /* !_KERN_ */

void intr_init(void);
void intr_enable(int irq, int cpunum);
void intr_global_enable(void);
void intr_global_disable(void);
void intr_eoi(void);

#endif /* !_KERN_INTR_H_ */
