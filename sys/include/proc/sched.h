#ifndef _KERN_SCHED_H_
#define _KERN_SCHED_H_

#ifdef _KERN_

#include <proc/proc.h>
#include <lib/queue.h>
#include <lib/spinlock.h>
#include <lib/types.h>

#include <dev/pcpu.h>

/*
 * Initialize the process scheduler.
 */
int sched_init(void);

/*
 * Reschedule the processes on the current processor.
 *
 * XXX: The scheduler on the current processor must be locked before entering
 *      sched_resched().
 *
 * @param select_new if TRUE, the scheduler will always select a process from
 *                   the ready queue if there's one.
 */
void sched_resched(bool select_new);

/*
 * Add a newly-created process to the scheduler on the specified processor which
 * will schedule the process in the future.
 *
 * XXX: The scheduler on the processor where the process will be running must be
 *      locked before entering sched_add().
 *
 * @param p the fresh process
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
void sched_add(struct proc *p);

/*
 * Wake the processes being sleeping on the specified resource.
 *
 * @param wchan the resource
 */
void sched_wake(void *wchan);

/*
 * Make a process sleeping on the specified resource.
 *
 * XXX: The scheduler on the current processor must be locked before entering
 *      sched_sleep().
 *
 * XXX: The process must be running on the current processor.
 *
 * @param p     the process to sleep
 * @param wchan the resource
 */
void sched_sleep(struct proc *p, void *wchan, spinlock_t *inv);

/*
 * Abandon the processor time.
 */
void sched_yield(void);

/*
 * Lock/Unlock the scheduler.
 */
void sched_lock(void);
void sched_unlock(void);

/*
 * Update scheduling information.
 */
void sched_update(void);

/*
 * Get the current process on a specified processor.
 *
 * @return the current process if there's any; otherwise, return NULL.
 */
struct proc *sched_cur_proc(void);

/*
 * Add a sleep queue for the specified resource.
 *
 * @param wchan the resource to block on
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int sched_add_slpq(void *wchan);

/*
 * Remove the sleep queue of the specified resource.
 *
 * @param wchan the resource to block on
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int sched_remove_slpq(void *wchan);

#endif /* _KERN_ */
#endif /* !_KERN_SCHED_H_ */
