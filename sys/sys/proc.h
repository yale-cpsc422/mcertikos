#ifndef _KERN_PROC_H_
#define _KERN_PROC_H_

#ifdef _KERN_

#include <sys/context.h>
#include <sys/gcc.h>
#include <sys/mqueue.h>
#include <sys/pcpu.h>
#include <sys/queue.h>
#include <sys/signal.h>
#include <sys/spinlock.h>
#include <sys/types.h>

#include <machine/pmap.h>
#include <machine/trap.h>

#define MAX_PID		64
#define MAX_MSG		8

#define PID_INV		(~(pid_t) 0)

/*
 *                              (3)
 *                      +----------------+
 *                      |                |
 *             (1)      V      (2)       |       (4)
 * PROC_INITED ---> PROC_READY ---> PROC_RUNNING ---> PROC_BLOCKED
 *      ^               ^                                 |
 *      |(0)            |              (5)                |
 *      |               +---------------------------------+
 *  PROC_INVAL
 *
 * (0) The process is initialzied.
 *
 * (1) The process structures and resources are initialized, and the process is
 *     ready to run.
 *
 * (2) The process is scheduled to run.
 *
 * (3) The process is time-out or yields to another process.
 *
 * (4) The process is blocked for events, e.g. waiting for an interrupt.
 *
 * (5) The process is unblocked, and ready to run again.
 */

typedef
volatile enum {
	PROC_INVAL,	/* invalid process */
	PROC_INITED,	/* process is initialized */
	PROC_READY,	/* process is ready to run */
	PROC_RUNNING,	/* process is running */
	PROC_BLOCKED	/* process is blocked and can't run */
} proc_state_t;

struct proc;

typedef int (*wake_cb_t) (struct proc *);

/*
 * Process Control Block.
 */
struct proc {
	pid_t		pid;	/* process id */
	pmap_t		*pmap;	/* page table */
	proc_state_t	state;	/* state of process */

	struct pcpu	*cpu;	/* which CPU I'm on */
	struct context	ctx;	/* process context */

	uint64_t	last_running_time;
	uint64_t	total_running_time;

	struct mqueue	mqueue;	/* message queue */

	spinlock_t	lk;	/* must be acquired before accessing this
				   structure */

	struct vm	*vm;	/* which virtual machine is running in this
				   process */

	/*
	 * A process can be in either of the free processes list, the ready
	 * processes list, the sleeping processes list or the dead processes
	 * list.
	 */
	TAILQ_ENTRY(proc) entry;
};

struct sched;

/*
 * Initialize the process module.
 *
 * @return 0 for success
 */
int proc_init(void);

/*
 * Initialize a process scheduler.
 */
int proc_sched_init(struct sched *);

/*
 * Acquire/Release the spinlock of a process. It's recommended to use them in
 * pairs.
 *
 * XXX: They are blocking operations, i.e. they are blocked until the operation
 *      succeeds.
 */
#define proc_lock(p)					\
	do {						\
		KERN_ASSERT(p != NULL);			\
		spinlock_acquire(&p->lk);		\
	} while (0)

#define proc_unlock(p)							\
	do {								\
		KERN_ASSERT(p != NULL);					\
		if (spinlock_holding(&p->lk) == FALSE)			\
			KERN_PANIC("Process %d tries to release "	\
				   "unlocked lock", p->pid);		\
		spinlock_release(&p->lk);				\
	} while (0)

/*
 * Create a new process.
 *
 * @param start the start address of the code to execute in this process
 *
 * @return a pointer to the structure of the process; NULL if failed.
 */
struct proc *proc_create(uintptr_t start);

/*
 * Block a process p.
 */
int proc_block(struct proc *p);

/*
 * Unblock a process p and put it on the ready queue.
 */
int proc_unblock(struct proc *p);

/*
 * Per-processor scheduler.
 */
void proc_sched(void) gcc_noreturn;

/*
 * Add a new created process to the scheduler.
 */
int proc_add2sched(struct proc *p);

/*
 * Yield to another process.
 */
int proc_yield(void);

/*
 * Get the current process.
 */
struct proc *proc_cur(void);

/*
 * Get the process by its process id.
 */
struct proc *proc_pid2proc(pid_t);

void
proc_save(struct proc *p, tf_t *tf);

#endif /* _KERN_ */

#endif /* !_KERN_PROC_H_ */
