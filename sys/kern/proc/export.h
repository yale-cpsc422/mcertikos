#ifndef _KERN_PROC_EXPORT_H_
#define _KERN_PROC_EXPORT_H_

#ifdef _KERN_

#include <lib/trap.h>
#include <lib/types.h>

#define PAGESIZE	4096

/*
 * User context.
 */

struct context {
	tf_t 		tf;	/* trapframe */
};

uint32_t ctx_errno(struct context *);
uint32_t ctx_arg1(struct context *);
uint32_t ctx_arg2(struct context *);
uint32_t ctx_arg3(struct context *);
uint32_t ctx_arg4(struct context *);
uint32_t ctx_arg5(struct context *);
uint32_t ctx_arg6(struct context *);
void ctx_set_errno(struct context *, uint32_t);
void ctx_set_retval1(struct context *, uint32_t);
void ctx_set_retval2(struct context *, uint32_t);
void ctx_set_retval3(struct context *, uint32_t);
void ctx_set_retval4(struct context *, uint32_t);
void ctx_set_retval5(struct context *, uint32_t);

/*
 * Process management.
 */

typedef int		pid_t;

struct proc {
	pid_t		pid;
	struct thread	*td;
	int		pmap_id;
	struct context	uctx;
	char		sysbuf[PAGESIZE];
	bool		inuse;
};

struct threadq {
	struct thread	*head;
	struct thread	*tail;
};

void proc_init(void);
struct proc *proc_create(uintptr_t elf_addr);
void proc_yield(void);
int proc_exit(void);
void proc_sleep(struct threadq *slpq);
void proc_wakeup(struct threadq *slpq);
struct proc *proc_cur(void);
void proc_save_uctx(struct proc *p, tf_t *tf);
void proc_start_user(void);
void thread_sched(void);

#endif /* _KERN_ */

#endif /* !_KERN_PROC_EXPORT_H_ */
