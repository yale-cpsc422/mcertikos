#ifndef _DEV_INTR_H_
#define _DEV_INTR_H_

#ifdef _KERN_

#include <lib/types.h>

void intr_init(void);
void intr_enable(uint8_t irq);
void intr_local_enable(void);
void intr_local_disable(void);
void intr_eoi(void);

#endif /* _KERN_ */

#endif /* !_DEV_INTR_H_ */
