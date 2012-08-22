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

struct proc;

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

typedef
enum {
	NOT_BLOCKED,	/* process is not blocked */
	WAIT_FOR_MSG	/* process is waiting for messages */
} block_reason_t;

typedef int (*unblock_cb_t) (struct proc *);

/*
 * Process Control Block.
 */
struct proc {
	pid_t		pid;	/* process id */
	pmap_t		*pmap;	/* page table */
	proc_state_t	state;	/* state of process */

	struct pcpu	*cpu;	/* which CPU I'm on */
	struct context	ctx;	/* process context */

	/*
	 * Whenever the process is blocked, blocked_for records the reason why
	 * it's blocked.
	 *
	 * If unblock_callback is not NULL, it will called once the blocked
	 * process is unblocked. unblock_callback is responsible to complete the
	 * blocking operations. For example, if there's no meesage in the
	 * message queue, receiving the message will block the receiver process.
	 * When new messages come, the process is unblocked and calls
	 * unblock_callback to get the message from the message queue.
	 */
	block_reason_t	blocked_for;
	unblock_cb_t	unblock_callback;

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
int proc_block(struct proc *p, block_reason_t);

/*
 * Unblock a process p and put it on the ready queue.
 */
int proc_unblock(struct proc *p);

/*
 * Per-processor scheduler.
 */
gcc_noreturn void proc_sched(void);

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

/*
 * Reschedule.
 */
int proc_resched(struct context *);

/*
 * Save the context of a process.
 */
void proc_save(struct proc *p, tf_t *tf);

/*
 * Send a messags to a process.
 *
 * @param receiver which process the message is sent to
 * @param msg      the message to be sent
 * @param size     the size of the message
 *
 * @return 0 if no errors; otherwise, return a non-zero value.
 */
int proc_send_msg(struct proc *receiver, void *msg, size_t size);

/*
 * Receive a message. If no message is present, the receiver process will be
 * blocked.
 *
 * @return the message if successful; if no message is present, return NULL.
 */
struct message *proc_recv_msg(void);

#endif /* _KERN_ */

#endif /* !_KERN_PROC_H_ */
