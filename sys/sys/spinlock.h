#ifndef _KERN_SPINLOCK_H_
#define _KERN_SPINLOCK_H_

#ifdef _KERN_

#include <sys/gcc.h>
#include <sys/types.h>
#include <sys/x86.h>

typedef volatile uint32_t spinlock_t;

#ifndef __COMPCERT__

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

static int gcc_inline
spinlock_try_acquire(spinlock_t *lk)
{
	return xchg(lk, 1);
}

static void gcc_inline
spinlock_release(spinlock_t *lk)
{
	if (spinlock_holding(lk) == FALSE)
		return;

	xchg(lk, 0);
}

#else /* !__COMPCERT__ */

extern gcc_inline void ccomp_spinlock_init(spinlock_t *lk);
extern gcc_inline bool ccomp_spinlock_holding(spinlock_t *lk);
extern gcc_inline void ccomp_spinlock_acquire(spinlock_t *lk);
extern gcc_inline int  ccomp_spinlock_try_acquire(spinlock_t *lk);
extern gcc_inline void ccomp_spinlock_release(spinlock_t *lk);

#define spinlock_init(lk)	ccomp_spinlock_init((lk))
#define spinlock_holding(lk)	ccomp_spinlock_holding((lk))
#define spinlock_acquire(lk)	ccomp_spinlock_acquire((lk))
#define spinlock_try_acquire(lk) ccomp_spinlock_try_acquire((lk))
#define spinlock_release(lk)	ccomp_spinlock_release((lk))

#endif /* __COMPCERT__ */

#endif /* _KERN_ */

#endif /* !_KERN_SPINLOCK_H_ */
