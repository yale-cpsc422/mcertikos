#ifndef _SYS_SYSCALL_H_
#define _SYS_SYSCALL_H_

#ifdef _KERN_

#define SYSCALL_PUTC			1
#define SYSCALL_GETC			2
#define SYSCALL_NCPU			3
#define SYSCALL_CPUSTAT			4
#define SYSCALL_SIGNAL			5
#define SYSCALL_SIGRET			6
#define SYSCALL_LOAD			7
#define SYSCALL_MGMT			8
#define SYSCALL_SETUPVM			9

#define SYSCALL_MGMT_START		1
#define SYSCALL_MGMT_STOP		2
#define SYSCALL_MGMT_ALLOCA_PAGE	3

#endif /* _KERN_ */

#endif /* !_SYS_SYSCALL_H_ */
