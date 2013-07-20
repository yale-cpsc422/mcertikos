#ifndef _KERN_THREAD_H_
#define _KERN_THREAD_H_

#ifdef _KERN_

#include <lib/export.h>

#include "kctx.h"
#include "kstack.h"

#define MAX_THREAD	MAX_KSTACK

typedef enum {
	TDS_READY,
	TDS_RUN,
	TDS_SLEEP,
	TDS_DEAD
} td_stat_t;

struct threadq;

struct thread {
	struct kctx	*td_kctx;
	td_stat_t	td_state;
	struct thread	*td_prev, *td_next;
	struct threadq	*td_slpq;
	struct kstack	*td_kstack;
	bool		inuse;
};

struct threadq {
	struct thread	*head;
	struct thread	*tail;
};

/*
 * Initialize the thread module.
 */
void thread_init(void);

/*
 * Thread scheduler.
 */
void thread_sched(void);

/*
 * Create a new thread.
 *
 * @param f the start function of the new thread
 *
 * @return a pointer to the thread structure if successful; otherwise, return
 *         NULL
 */
struct thread *thread_spawn(void (*f)(void));

/*
 * Kill another thread.
 *
 * @prarm td the thread to kill
 *
 * @return 0 if successful; otherwise, return a non-zero value
 */
int thread_kill(struct thread *td);

/*
 * Exit the current thread.
 */
void thread_exit(void);

/*
 * Yield to other threads.
 */
void thread_yield(void);

/*
 * Put the current thread on a sleep queue.
 *
 * @param sq the sleep queue
 */
void thread_sleep(struct threadq *slpq);

/*
 * Wakeup all threads in a sleep queue.
 *
 * @oaram slpq the sleep queue
 */
void thread_wakeup(struct threadq *slpq);

#endif /* _KERN_ */

#endif /* !_KERN_THREAD_H_ */
