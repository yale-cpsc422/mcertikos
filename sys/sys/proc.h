#ifndef _KERN_PROC_H_
#define _KERN_PROC_H_

#ifdef _KERN_

#include <sys/channel.h>
#include <sys/context.h>
#include <sys/gcc.h>
#include <sys/pcpu.h>
#include <sys/queue.h>
#include <sys/signal.h>
#include <sys/spinlock.h>
#include <sys/trap.h>
#include <sys/types.h>

#include <machine/pmap.h>

#define MAX_PID		64
#define MAX_MSG		8

#define PID_INV		(~(pid_t) 0)

struct proc;
typedef int (*trap_cb_t) (struct context *);

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
 * (1) A new process is put on the ready queue.
 * (2) A ready process is scheduled to run.
 * (3) A running process runs out of its time slice or gives up the CPU.
 * (4) A running process is blocked.
 * (5) A blocked process is unblocked.
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
	WAITING_FOR_SENDING,	/* process is waiting for the completion of the
				   message sending */
	WAITING_FOR_RECEIVING	/* process is waiting for the completion of the
				   message receiving */
} block_reason_t;

typedef int (*unblock_cb_t) (struct proc *);

/*
 * Process Control Block (PCB)
 *
 * Protection categories:
 * - *: not protected yet
 * - c: created at proc_spawn(), never changed in the future
 * - a: protected by the process lock
 * - s: protected by the scheduler lock
 */
struct proc {
	pid_t		pid;	/* (c) process id */
	struct kstack	*kstack;/* (c) kernel stack for this process */
	pmap_t		*pmap;	/* (c) page table */
	proc_state_t	state;	/* (s) state of process */

	struct pcpu	*cpu;	/* (c) which CPU I'm on */
	struct kern_ctx	*kctx;	/* (a) kernel context for kernel context
				   switches */
	struct context	ctx;	/* (a) user context for traps  */

	struct vm	*vm;	/* (a) which virtual machine is running in this
				   process */

	block_reason_t	block_reason;	/* (s) why the process is blocked */
	struct channel	*block_channel;	/* (s) which channel the process is
					   blocked on */

	struct proc	*parent;    /* (s) the parent process of this process */
	struct channel	*parent_ch; /* (a) bidirection channel to the parent */

	/*
	 * (s) child_list: all children process of this process
	 * (s) child_entry: child_list entry in the parent's child_list
	 */
	TAILQ_HEAD(children, proc) child_list;
	TAILQ_ENTRY(proc)          child_entry;

	spinlock_t	lk;	/* (*) process lock */

	/*
	 * A process can be in either of the free processes list, the ready
	 * processes list, the sleeping processes list or the dead processes
	 * list.
	 */
	TAILQ_ENTRY(proc) entry;/* (s) linked list entry in scheduler queue */
};

#define proc_lock(p)				\
	do {					\
		spinlock_acquire(&p->lk);	\
	} while (0)

#define proc_unlock(p)				\
	do {					\
		spinlock_release(&p->lk);	\
	} while (0)

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
 * Start running a new process on a specified processor.
 *
 * proc_spawn() creates a new process, load an execution file to the address
 * space of the process, and notifies the scheduler on the specified processor
 * to run the process.
 *
 * @param c     the processor on which the process will run
 * @param start the start address of the execution file.
 *
 * @return PCB of the process if spawn succeeds; otherwise, return NULL.
 */
struct proc *proc_spawn(struct pcpu *c, uintptr_t start);

/*
 * Block a process.
 *
 * @param p      which process is to be blocked
 * @param reason why the process is blocked
 * @param ch     which channel the process is blocked on
 */
void proc_block(struct proc *p, block_reason_t reason, struct channel *ch);

/*
 * Unblock a process.
 *
 * @param p which process is to be unblocked
 */
void proc_unblock(struct proc *p);

/*
 * Per-processor scheduler.
 */
void proc_sched(bool need_sched);

/*
 * Update the scheduling information.
 */
void proc_sched_update(void);

/*
 * Yield to another process.
 */
void proc_yield(void);

/*
 * Get the current process.
 */
struct proc *proc_cur(void);

/*
 * Get the process by its process id.
 */
struct proc *proc_pid2proc(pid_t);

/*
 * Save the context of a process.
 */
void proc_save(struct proc *p, tf_t *tf);

/*
 * Send a messags through a channel. If the channel is busy when proc_send_msg()
 * is trying to send the message, the sending process will be blocked util the
 * channel is idle.
 *
 * @param ch   the channel through which the message will be sent
 * @param msg  the message to be sent
 * @param size the size of the message
 *
 * @return 0 if the sending succeeds; otherwise,
 *  return E_CHANNEL_BUSY, if there's pending messages in the channel;
 *  return E_CHANNEL_ILL_TYPE, if the channel type is receive-only;
 *  return E_CHANNEL_MSG_TOO_LARGE, if the message is too large;
 *  return E_CHANNEL_ILL_SENDER, if the sending process is not allowed to send
 *         through the channel.
 */
int proc_send_msg(struct channel *ch, void *msg, size_t size);

/*
 * Receive a message through a channel. If the channel is idle when
 * proc_recv_msg() is trying to receive the message, the receiving process will
 * be blocked until the channel is busy.
 *
 * @param ch   the channel from which the message will be received
 * @param msg  where the received message will be stored
 * @param size where the size of the received message will be stored
 *
 * @return 0 if the receiving succeeds; otherwise,
 *  return E_CHANNEL_IDLE, if there's no message in the channel;
 *  return E_CHANNEL_ILL_TYPE, if the channel type is send-only;
 *  return E_CHANNEL_ILL_RECEIVER, if the receiving process is not allowed to
 *         receive from the channel.
 */
int proc_recv_msg(struct channel *ch, void *msg, size_t *size);

#endif /* _KERN_ */

#endif /* !_KERN_PROC_H_ */
