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
 * States and state transitions of a process:
 *
 * PROC_RUNNING   : the process is running and a currently running process on a
 *                  processor
 * PROC_READY     : the process is ready to be scheduled to run
 * PROC_SLEEPING  : the process is sleeping, usually for waiting for some
 *                  resource
 * PROC_DEAD      : the process is dead and waiting for recycle, usually for
 *                  being killed or terminating normally
 *
 *                          +---------------+
 *                          | PROC_UNINITED |
 *                          +---------------+
 *                                  |
 *                                  | a process is created
 *                                  V
 *                           +-------------+
 *                           | PROC_INITED |
 *                           +-------------+
 *                                  | put in a ready queue
 *                                  |
 *           be scheduled           V         some resource
 *             or yield      +------------+     unavailable
 *       +-----------------> | PROC_READY | ------------------+
 *       |  +--------------- +------------+ <--------------+  |
 *       |  |                       |                      |  |
 *       |  | be scheduled          |        some resource |  |
 *       |  |                       |          available   |  |
 *       |  V        some resource  |                      |  V
 * +--------------+   unavailable   |                +---------------+
 * | PROC_RUNNING | ----------------(--------------> | PROC_SLEEPING |
 * +--------------+                 |                +---------------+
 *         \                        |                        /
 *          |                       |                       |
 *           \----------------------+----------------------/
 *                                  | terminate normally
 *                                  |    or be killed
 *                                  V
 *                             +-----------+
 *                             | PROC_DEAD |
 *                             +-----------+
 */

typedef
volatile enum {
	PROC_UNINITED,	/* process is not created */
	PROC_INITED,	/* process is created */
	PROC_READY,	/* process can run but not running now */
	PROC_RUNNING,	/* process is running */
	PROC_SLEEPING,	/* process is sleeping for some resource */
	PROC_DEAD	/* process is killed or terminates normally */
} proc_state_t;

struct proc;

typedef int (*wake_cb_t) (struct proc *);

struct proc {
	pid_t		pid;	/* process id */
	pmap_t		*pmap;	/* page table */
	proc_state_t	state;	/* state of process */

	struct pcpu	*cpu;	/* which CPU I'm on */
	struct context	ctx;	/* process context */

	spinlock_t	*waiting_for;
	wake_cb_t	wake_cb;

	uint64_t	start_time;	/* when did this process start
					   running? */

	struct mqueue	mqueue;	/* message queue */

	spinlock_t	lk;	/* must be acquired before accessing this
				   structure */

	struct vm	*vm;

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
void proc_lock(struct proc *proc);
void proc_unlock(struct proc *proc);

/*
 * Create a new process.
 *
 * @param start the start address of the code to execute in this process
 *
 * @return a pointer to the structure of the process; NULL if failed.
 */
struct proc *proc_create(uintptr_t start);

/*
 * Put a process p on the ready queue of the cpu c.
 */
int proc_ready(struct proc *p, struct pcpu *c);

/*
 * Let a process p sleep to wait for the spinlock lk.
 */
int proc_sleep(struct proc *p, spinlock_t *lk, wake_cb_t);

/*
 * Wake up a sleeping process.
 */
int proc_wake(struct proc * p);

/*
 * Yield a running process p.
 */
int proc_yield(struct proc *p);

/*
 * Per-cpu process scheduler.
 */
int proc_sched(struct pcpu *c);

/*
 * Start running process p.
 */
void proc_run(void) gcc_noreturn;

/* Get the current process. */
struct proc *proc_cur(void);

/* save the context of the process */
void proc_save(struct proc *, tf_t *);

/* get the process by its process id */
struct proc *proc_pid2proc(pid_t);

#endif /* _KERN_ */

#endif /* !_KERN_PROC_H_ */
