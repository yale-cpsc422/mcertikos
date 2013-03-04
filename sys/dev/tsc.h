#ifndef _SYS_DEV_TSC_H_
#define _SYS_DEV_TSC_H_

#ifdef _KERN_

#include <sys/types.h>

#ifndef __COMPCERT__
volatile uint64_t tsc_per_ms;	/* TSC ticks per microsecond */
#endif

int tsc_init(void);
void delay(uint32_t);

uint64_t time_ms(void);

#ifdef __COMPCERT__
uint64_t *ccomp_tsc_per_ms(void);
#endif

#endif /* _KERN_ */

#endif /* !_SYS_DEV_TSC_H_ */
