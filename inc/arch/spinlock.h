// Spinlock primitive for mutual exclusion within the kernel.
// Adapted from xv6.
// See COPYRIGHT for copyright information.
#ifndef PIOS_KERN_SPINLOCK_H
#define PIOS_KERN_SPINLOCK_H

#include <inc/arch/types.h>

// Mutual exclusion lock.
typedef uint32_t spinlock; // spinlock is just a number


/*
typedef struct spinlock {
	uint32_t locked;	// Is the lock held?
	uint32_t* debug;    // pointer to debugging information
} spinlock;
*/

void spinlock_init(spinlock *lk);
void spinlock_acquire(spinlock *lk);
void spinlock_release(spinlock *lk);
int spinlock_holding(spinlock *lk);

#endif /* !PIOS_KERN_SPINLOCK_H */
