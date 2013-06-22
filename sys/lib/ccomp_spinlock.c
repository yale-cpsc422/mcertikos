#include <lib/gcc.h>
#include <lib/spinlock.h>
#include <lib/types.h>
#include <lib/x86.h>

gcc_inline void
ccomp_spinlock_init(spinlock_t *lk)
{
	*lk = 0;
}

gcc_inline bool
ccomp_spinlock_holding(spinlock_t *lock)
{
	return *lock;
}

gcc_inline void
ccomp_spinlock_acquire(spinlock_t *lk)
{
	while(xchg(lk, 1) != 0)
		pause();
}

gcc_inline int
ccomp_spinlock_try_acquire(spinlock_t *lk)
{
	return xchg(lk, 1);
}

gcc_inline void
ccomp_spinlock_release(spinlock_t *lk)
{
	if (spinlock_holding(lk) == FALSE)
		return;

	xchg(lk, 0);
}
