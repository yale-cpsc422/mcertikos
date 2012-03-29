#ifndef _SYS_TIMER_H_
#define _SYS_TIMER_H_

#ifdef _KERN_

#include <sys/spinlock.h>
#include <sys/types.h>

typedef uint32_t timer_t;

#define INVALID_TIMER	(~(uint32_t) 0x0)

void timer_init(void);
void timer_handle_timeout(void);

timer_t timer_add(uint64_t expire_time,
		  void (*handler)(void *, uint64_t), void *params);
int timer_remove(timer_t);
int timer_update_expire_time(timer_t, uint64_t expire_time);

int timer_enable(timer_t);
int timer_disable(timer_t);

bool timer_is_enabled(timer_t);

#endif /* _KREN_ */

#endif /* !_SYS_TIMER_H_ */
