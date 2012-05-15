#ifndef _KERN_PROC_H_
#define _KERN_PROC_H_

#ifdef _KERN_

#include <sys/context.h>
#include <sys/gcc.h>
#include <sys/msg.h>
#include <sys/pcpu.h>
#include <sys/signal.h>
#include <sys/spinlock.h>
#include <sys/types.h>

#include <machine/pmap.h>

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

typedef struct pcpu_t pcpu_t;

typedef
struct proc_t {
	spinlock_t	lk;	/* must be acquired before accessing this
				   structure */

	pid_t		pid;	/* process id */

	volatile proc_state_t state;	/* state of process */

	pcpu_t		*cpu;	/* which CPU I'm on */

	pmap_t		*pmap;	/* page table */

	struct context_t  *normal_ctx;	/* context for normal execution */
	struct context_t  *signal_ctx;	/* context for handling interrupts */

	mqueue_t	mqueue;	/* message queue */

	struct proc_t	*next;	/* next process of the same state */
} proc_t;

void proc_init(void);

pid_t proc_new(uintptr_t);
void proc_start(pid_t) gcc_noreturn;
void proc_stop(pid_t);

int proc_send_msg(pid_t, msg_type_t, void *data, size_t);
msg_t *proc_recv_msg(pid_t);

pmap_t *proc_pmap(pid_t);
proc_state_t proc_state(pid_t);
pcpu_t *proc_cpu(pid_t);

void proc_lock(pid_t);
void proc_unlock(pid_t);

#endif /* _KERN_ */

#endif /* !_KERN_PROC_H_ */
