#ifndef _KERN_INTR_H_
#define _KERN_INTR_H_

#ifdef _KERN_

#include <sys/x86.h>

#define intr_local_enable()			\
	do {					\
		sti();				\
	} while (0);

#define intr_local_disable()			\
	do {					\
		cli();				\
	} while(0)

void intr_init(void);
void intr_enable(uint8_t irq, int cpunum);

#define intr_enable_all(irq)			\
	do {					\
		intr_enable((irq), 0xff);	\
	} while (0)

void intr_eoi(void);

#endif /* _KERN_ */

#endif /* !_KERN_INTR_H_ */
