#ifndef _KERN_SCHED_H_
#define _KERN_SCHED_H_

#ifdef _KERN_

#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/spinlock.h>
#include <sys/types.h>

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
 * @param c if not NULL, the process is pinned on that processor.
 *          XXX: the current implementation always pins a process on one
 *               processor and therefore requires c != NULL
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
void sched_add(struct proc *p, struct pcpu *c);

/*
 * Wake a sleeping process.
 *
 * XXX: The scheduler on the processor where the sleeping process is must be
 *      locked before entering sched_wake().
 *
 * @param p the sleeping process to be waken
 */
void sched_wake(struct proc *p);

/*
 * Make a the currently running process to sleep.
 *
 * XXX: The scheduler on the current processor must be locked before entering
 *      sched_sleep().
 *
 * XXX: The process must be running on the current processor.
 *
 * @param p the process to sleep
 */
void sched_sleep(struct proc *p);

/*
 * Abandon the processor time.
 */
void sched_yield(void);

/*
 * Lock/Unlock the scheduler on the specified processor.
 *
 * @param c the processor
 */
void sched_lock(struct pcpu *c);
void sched_unlock(struct pcpu *c);

/*
 * Update scheduling information.
 */
void sched_update(void);

/*
 * Get the current process on a specified processor.
 *
 * @param c the processor
 *
 * @return the current process if there's any; otherwise, return NULL.
 */
struct proc *sched_cur_proc(struct pcpu *c);

#endif /* _KERN_ */
#endif /* !_KERN_SCHED_H_ */
