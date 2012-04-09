#ifndef _SYS_SYSCALL_H_
#define _SYS_SYSCALL_H_

/*
* syscall header for certikos
* basically for master and client kernel
*
*
*
*/
// Master syscall

/* must be identity to T_SYSCALL defined in sys/arch/xxx/include/trap.h */
#define T_SYSCALL		48

#define SYSCALL_PUTS			1
#define SYSCALL_GETC			2
#define SYSCALL_NCPU			3
#define SYSCALL_CPUSTAT			4
#define SYSCALL_SIGNAL			5
#define SYSCALL_SIGRET			6
#define SYSCALL_LOAD			7
#define SYSCALL_MGMT			8
#define SYSCALL_STARTUPVM		9

#define SYSCALL_MGMT_START		1
#define SYSCALL_MGMT_STOP		2
#define SYSCALL_MGMT_ALLOCA_PAGE	3


//client syscall
// The number of the interrupt on which the syscalls happend on application cores
#define T_CLIENT_SYSCALL 48

#define SYSCALL_CLIENT_PUTS 1
#define SYSCALL_CLIENT_GETC 2
#define SYSCALL_CLIENT_TIME 3
#define SYSCALL_CLIENT_PID 4
#define SYSCALL_CLIENT_CPU 5
#define SYSCALL_CLIENT_SETUPVM 6

//#endif /* _KERN_ */

#endif /* !_SYS_SYSCALL_H_ */
