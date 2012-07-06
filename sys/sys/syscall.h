#ifndef _SYS_SYSCALL_H_
#define _SYS_SYSCALL_H_

#ifdef _KERN_

#include <sys/context.h>
#include <sys/types.h>

#include <machine/trap.h>

int syscall_handler(struct context *);

#endif /* _KERN_ */

#define T_SYSCALL	48

/*
 * System calls for I/O
 */
#define SYS_puts	0	/* output a string to the console */
#define SYS_getc	1	/* input a character from the console */

/*
 * Systam calls for the process
 */
#define SYS_spawn	2	/* spawn a process */
#define SYS_yield	3	/* yield to another process */
#define SYS_getpid	4	/* get my process id */
#define SYS_send	5	/* send a message to another process */
#define SYS_recv	6	/* receive a message */

/*
 * System calls for the virtual machine
 */
#define SYS_allocvm	32	/* allocate a virtual machine */
#define SYS_execvm	33	/* execute the virtual machine */

/*
 * System calss for testing
 */
#define SYS_test	255

#endif /* !_SYS_SYSCALL_H_ */
