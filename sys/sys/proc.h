#ifndef _KERN_PROC_H_
#define _KERN_PROC_H_

#ifdef _KERN_

#include <sys/as.h>
#include <sys/context.h>
#include <sys/gcc.h>
#include <sys/msg.h>
#include <sys/signal.h>
#include <sys/spinlock.h>
#include <sys/types.h>

#define NMSG	8

typedef
enum {
	PROC_STOP,	/* process is stopped, such as interrupted or waiting
			   for some signals */
	PROC_READY,	/* process can run but not run */
	PROC_RUN,	/* process is running */
	PROC_INSIG,	/* process is handling signals */
	PROC_DEAD	/* process is dead and waiting for cleared */
} proc_state_t;

typedef
struct proc_t {
	spinlock_t	lk;	/* must be acquired before accessing this
				   structure */

	pid_t		pid;	/* process id */

	proc_state_t	state;	/* state of process */

	as_t		*as;	/* address space of this process */

	struct context_t  *normal_ctx;	/* context for normal execution */
	struct context_t  *signal_ctx;	/* context for handling interrupts */

	mqueue_t	mqueue;	/* message queue */

	struct proc_t	*next;	/* next process of the same state */
} proc_t;

void proc_init(void);

pid_t proc_new(uintptr_t);
void proc_start(pid_t) gcc_noreturn;

int proc_send_msg(pid_t, msg_type_t, void *data, size_t);
msg_t *proc_recv_msg(pid_t);

as_t *proc_as(pid_t);

void proc_lock(pid_t);
void proc_unlock(pid_t);

#endif /* _KERN_ */

#endif /* !_KERN_PROC_H_ */
