#ifndef _PREINIT_LIB_TIMING_H_
#define _PREINIT_LIB_TIMING_H_

#include <preinit/lib/types.h>
#include <preinit/dev/timer.h>

extern unsigned long long jiffies;

typedef uint32_t
(*run) (void *);

typedef struct runnable
{
    int enable;
    int period;
    void * param;
    run action;
} runnable_t;

/**
 * 1 cycle time of period (in second) needs IRQ_PERIOD(x) irqs
 */
#define IRQ_PERIOD(x)     (FREQ * (x))

void
periodic (void);

#endif /* !_PREINIT_LIB_TIMING_H_ */

