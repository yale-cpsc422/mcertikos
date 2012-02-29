#ifndef _KERN_SPINLOCK_H_
#define _KERN_SPINLOCK_H_

#ifdef _KERN_

#include <sys/gcc.h>
#include <sys/types.h>
#include <sys/x86.h>

typedef volatile uint32_t spinlock_t;

static void gcc_inline
spinlock_init(spinlock_t *lk)
{
	*lk = 0;
}

static bool gcc_inline
spinlock_holding(spinlock_t *lock)
{
	return *lock;
}

static void gcc_inline
spinlock_acquire(spinlock_t *lk)
{
	while(xchg(lk, 1) != 0)
		pause();
}

static void gcc_inline
spinlock_release(spinlock_t *lk)
{
	if (spinlock_holding(lk) == FALSE)
		return;

	xchg(lk, 0);
}

#endif /* _KERN_ */

#endif /* !_KERN_SPINLOCK_H_ */
