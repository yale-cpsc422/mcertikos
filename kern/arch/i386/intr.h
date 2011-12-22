#ifndef CERTIKOS_KERN_INTR_H
#define CERTIKOS_KERN_INTR_H

void intr_init(void);
void intr_enable(int irq, int cpunum);
void intr_eoi(void);

#endif /* !CERTIKOS_KERN_INTR_H */
