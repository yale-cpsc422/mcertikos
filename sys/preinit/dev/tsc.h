#ifndef _SYS_PREINIT_DEV_TSC_H_
#define _SYS_PREINIT_DEV_TSC_H_

#ifdef _KERN_

#include <preinit/lib/types.h>

volatile uint64_t tsc_per_ms;	/* TSC ticks per microsecond */

int tsc_init(void);
void delay(uint32_t);

#endif /* _KERN_ */

#endif /* !_SYS_PREINIT_DEV_TSC_H_ */
