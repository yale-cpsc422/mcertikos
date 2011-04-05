// Mutual exclusion spin locks.
// Adapted from xv6.

#include <inc/arch/x86.h>
#include <inc/arch/spinlock.h>

#include <kern/debug/debug.h>

void
spinlock_init(spinlock *lk)
{
	*lk = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
void
spinlock_acquire(spinlock *lk)
{
//	if(spinlock_holding(lk))
//		panic("recursive spinlock_acquire");

	// The xchg is atomic.
	// It also serializes,
	// so that reads after acquire are not reordered before it. 
	while(xchg(lk, 1) != 0)
		pause();	// let CPU know we're in a spin loop

	// Record info about lock acquisition for debugging.
//	lk->cpu = cpu_cur();
//	debug_trace(read_ebp(), lk->eips);
}

// Release the lock.
void
spinlock_release(spinlock *lk)
{
	assert(spinlock_holding(lk));

	// The xchg serializes, so that reads before release are 
	// not reordered after it.  The 1996 PentiumPro manual (Volume 3,
	// 7.2) says reads can be carried out speculatively and in
	// any order, which implies we need to serialize here.
	// But the 2007 Intel 64 Architecture Memory Ordering White
	// Paper says that Intel 64 and IA-32 will not move a load
	// after a store. So lock->locked = 0 would work here.
	// The xchg being asm volatile ensures gcc emits it after
	// the above assignments (and after the critical section).
	xchg(lk, 0);
}

// Check whether this cpu is holding the lock.
int
spinlock_holding(spinlock *lock)
{
	return *lock;
}
