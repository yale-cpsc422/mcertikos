#ifndef _KERN_PROC_H_
#define _KERN_PROC_H_

#ifdef _KERN_

#include <lib/export.h>

#include "context.h"
#include "thread.h"

#define PAGESIZE	4096
#define MAX_PROC	MAX_THREAD

typedef int		pid_t;

struct proc {
	pid_t		pid;
	struct thread	*td;
	int		pmap_id;
	struct context	uctx;
	char		sysbuf[PAGESIZE];
	bool		inuse;
};

/*
 * Initialize the process module.
 */
void proc_init(void);

/*
 * Create a new process and load an ELF image to its userspace.
 *
 * @param elf_addr the address of the ELF image
 *
 * @return a pointer to the process structure if successful; otherwise, return
 *         NULL
 */
struct proc *proc_create(uintptr_t elf_addr);

/*
 * Yield to other processes.
 */
void proc_yield(void);

/*
 * XXX: Not implemetned yet!!!
 */
int proc_exit(void);

/*
 * XXX: Not implemented yet!!!
 */
int proc_terminate(struct proc *p);

/*
 * Make the current process sleeping on a specified sleep queue.
 *
 * @param p    the process to sleep
 * @param slpq the sleep queue
 */
void proc_sleep(struct threadq *slpq);

/*
 * Wake up all processes sleeping on a specified sleep queue.
 *
 * @param slpq the sleep queue
 */
void proc_wakeup(struct threadq *slpq);

/*
 * Get the current process.
 */
struct proc *proc_cur(void);

/*
 * Save the trapframe in the user context of the specified process.
 *
 * @param p  the process
 * @param tf the trapfram to save
 */
void proc_save_uctx(struct proc *p, tf_t *tf);

/*
 * Start the user-space of the current process.
 */
void proc_start_user(void);

#endif /* _KERN_ */

#endif /* !_KERN_PROC_H_ */
