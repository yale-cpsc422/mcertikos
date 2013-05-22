#ifndef _KERN_PROC_H_
#define _KERN_PROC_H_

#ifdef _KERN_

#include <sys/channel.h>
#include <sys/context.h>
#include <sys/gcc.h>
#include <sys/pcpu.h>
#include <sys/queue.h>
#include <sys/spinlock.h>
#include <sys/trap.h>
#include <sys/types.h>

#include <machine/pmap.h>

#define MAX_PID		64
#define MAX_MSG		8

struct proc;

/*
 *                              (3)
 *                      +----------------+
 *                      |                |
 *             (1)      V      (2)       |       (4)
 * PROC_INITED ---> PROC_READY ---> PROC_RUNNING ---> PROC_SLEEPING
 *      ^               ^                                 |
 *      |(0)            |              (5)                |
 *      |               +---------------------------------+
 *  PROC_INVAL
 *
 * (1) A new process is put on the ready queue.
 * (2) A ready process is scheduled to run.
 * (3) A running process runs out of its time slice or gives up the CPU.
 * (4) A running process is sleeping.
 * (5) A blocked process is waken.
 */

typedef
volatile enum {
	PROC_INVAL,	/* invalid process */
	PROC_INITED,	/* process is initialized */
	PROC_READY,	/* process is ready to run */
	PROC_RUNNING,	/* process is running */
	PROC_SLEEPING	/* process is blocked and can't run */
} proc_state_t;

/*
 * Process Control Block (PCB)
 *
 * Locks categories:
 * (s): protected by the scheduler lock
 * (p): protected by the process lock
 * (?): no protection
 */
struct proc {
	spinlock_t	proc_lk;	/* (?) process lock */

	struct pcpu	*cpu;		/* (?) which processor I'm currently on */

	pid_t		pid;		/* (p) process identity */

	struct proc	*parent;	/* (p) parent process */
	TAILQ_HEAD(children, proc) children; /* (p) child processes */
	TAILQ_ENTRY(proc) child;	/* (p) entry in parent's children */

	struct channel	*parent_ch;	/* (?) bootstrap channel to parent */

	pmap_t		*pmap;		/* (p) user page structions */
	struct context	uctx;		/* (p) user context */
	uint8_t		*sys_buf;	/* (p) buffer for handling system calls */

	proc_state_t	state;		/* (s) current state */

	struct kstack	*kstack;	/* (s) kernel stack */
	struct kern_ctx	*kctx;		/* (s) kernel context */

	spinlock_t	*inv;		/* (?) invariant */

	TAILQ_ENTRY(proc) entry;	/* (s) entry in scheduler queues */
};

#define proc_lock(p)				\
	do {					\
		spinlock_acquire(&p->proc_lk);	\
	} while (0)

#define proc_unlock(p)				\
	do {					\
		spinlock_release(&p->proc_lk);	\
	} while (0)

struct sched;

/*
 * Initialize the process module.
 *
 * @return 0 for success
 */
int proc_init(void);

/*
 * Create a process. The sending permission of a channel will be granted to
 * the newly created process.
 *
 * @param parent the parent process; or NULL if this is the first process
 * @param ch     the channel
 *
 * @return PCB of the new process if successful; otherwise, return NULL.
 */
struct proc *proc_new(struct proc *parent, struct channel *ch);

/*
 * Execute the user code in a process. The code is from a ELF image.
 *
 * XXX: Current version of CertiKOS lacks file systems and is linked with the
 *      user ELF files.
 *
 * @param p     the process
 * @param c     which processor the process will execute on
 * @param u_elf where ELF image of the user code is
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int proc_exec(struct proc *p, struct pcpu *c, uintptr_t u_elf);

/*
 * Make a process to sleep. If an invariant lock is acquired when calling
 * proc_sleep(), it will be released when switching to another process and be
 * acquired again when the sleeping process is awaken. It's the caller's
 * responsibility to avoid deadlocks.
 *
 * XXX: proc_sleep() releases the lock inv after the process sleeps, and regains
 *      it after the process wakes up.
 *
 * XXX: Releasing inv and making the process sleeping are made as an atomic
 *      operation, so are regaining inv and waking up the process.
 *
 * XXX: If inv != NULL, it must be acquired before entering proc_sleep().
 *
 * @param p     the process to be sleeping
 * @param wchan the resource on which the process is going to sleep
 * @param inv   the invariant lock
 */
void proc_sleep(struct proc *p, void *wchan, spinlock_t *inv);

/*
 * Wake the processes sleeping on the specified resource.
 *
 * @param wchan the resource on which the processes are sleeping
 */
void proc_wake(void *wchan);

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
 * Get the channel to parent.
 */
struct channel *proc_parent_channel(struct proc *p);

#endif /* _KERN_ */

#endif /* !_KERN_PROC_H_ */
