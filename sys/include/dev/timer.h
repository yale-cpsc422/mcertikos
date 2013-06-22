#ifndef _KERN_DEV_TIMER_H_
#define _KERN_DEV_TIMER_H_

#ifndef _KERN_
#error "This is a kernel header file; do not include it in userspace programs."
#endif

#include <lib/types.h>

#define	TIMER_FREQ	1193181.182
#define CAL_MS		10
#define CAL_LATCH	(TIMER_FREQ / (1000 / CAL_MS))
#define CAL_PIT_LOOPS	1000

void timer_hw_init(void);
uint64_t timer_read(void);

#endif /* !_KERN_DEV_TIMER_H_ */
