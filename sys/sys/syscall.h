#ifndef _SYS_SYSCALL_H_
#define _SYS_SYSCALL_H_

#ifdef _KERN_

#include <sys/context.h>
#include <sys/types.h>

#include <machine/trap.h>

int syscall_handler(struct context *);

#endif /* _KERN_ */

#define T_SYSCALL	48

#define SYS_puts	0	/* output a string to the console */
#define SYS_getc	1	/* input a character from the console */

#define SYS_spawn	2	/* spawn a process */
#define SYS_yield	3	/* yield to another process */
#define SYS_getpid	4	/* get my process id */
#define SYS_send	5	/* send a message to another process */
#define SYS_recv	6	/* receive a message */

#define SYS_startvm	9	/* start a virtual machine */

#define SYS_test	255

#endif /* !_SYS_SYSCALL_H_ */
