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
#define SYS_ncpus	7	/* get the number of processors */
#define SYS_getpchid	8	/* get the channel if to the parent process */

/*
 * System calls for the virtual machine
 */
#define SYS_allocvm	32	/* allocate a virtual machine */
#define SYS_execvm	33	/* execute the virtual machine */

/*
 * System calss for testing
 */
#define SYS_test	255

/*
 * Error codes
 */
#define E_SUCC		0	/* no error */
/* common error codes  */
#define E_INVAL_NR	1	/* invalid system call */
#define E_MEM_FAIL	2	/* memory failure */
/* error codes specific for SYS_spawn */
#define E_SPAWN_FAIL	3	/* fail to spawn a process */
/* error codes specific for SYS_allocvm */
#define E_VM_ON_AP	4	/* not on bootstrap processor */
#define E_VM_EXIST	5	/* a VM is already there */
#define E_VM_INIT_FAIL	6	/* fail to initialize the VM */
/* error codes specific for SYS_execvm */
#define E_NO_VM		7	/* no VM */
/* error codes specific for SYS_getpchid */
#define E_INVAL_PROC	8	/* caller is not an valid process */
#define E_NO_CHANNEL	9	/* no channel */
/* error codes specific for SYS_send & SYS_recv */
#define E_SEND_BUSY	10	/* the channel is busy */
#define E_RECV_IDLE	11	/* the channel is idle */
#define E_NO_PERM	12	/* the caller has no permission */
#define E_LARGE_MSG	13	/* the message is too large */
#define E_EMPTY_MSG	14	/* the message is empty */

#endif /* !_SYS_SYSCALL_H_ */
