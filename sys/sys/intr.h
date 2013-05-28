#ifndef _KERN_INTR_H_
#define _KERN_INTR_H_

#ifdef _KERN_

#include <sys/types.h>
#include <sys/x86.h>

#ifndef __COMPCERT__

#define intr_local_enable()			\
	do {					\
		sti();				\
	} while (0);

#define intr_local_disable()			\
	do {					\
		cli();				\
	} while(0)

#else /* !__COMPCERT__ */

extern void ccomp_intr_local_enable(void);
extern void ccomp_intr_local_disable(void);

#define intr_local_enable()	ccomp_intr_local_enable()
#define intr_local_disable()	ccomp_intr_local_disable()

#endif

void intr_init(void);
void intr_enable(uint8_t irq);

#define intr_enable_all(irq)			\
	do {					\
		intr_enable((irq), 0xff);	\
	} while (0)

void intr_eoi(void);

#endif /* _KERN_ */

#endif /* !_KERN_INTR_H_ */
