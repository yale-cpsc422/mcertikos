#ifndef _USER_SYSCALL_H_
#define _USER_SYSCALL_H_

#include <gcc.h>
#include <types.h>

/* must be identity to T_SYSCALL defined in sys/arch/xxx/include/trap.h */
#define T_SYSCALL		48

/* must be identity to those defined in sys/sys/user.h */
#define SYSCALL_PUTC		1
#define SYSCALL_GETC		2
#define SYSCALL_NCPU		3
#define SYSCALL_CPUSTAT		4
#define SYSCALL_SIGNAL		5
#define SYSCALL_SIGRET		6
#define SYSCALL_LOAD		7
#define SYSCALL_MGMT		8
#define SYSCALL_STARTUPVM		9

/* must be identity to those defined in sys/sys/user.h */
#define MGMT_START		1
#define MGMT_STOP		2
#define MGMT_ALLOCA_PAGE	3

static void gcc_inline
sys_putc(const char *c)
{
	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYSCALL_PUTC),
		       "b" (c)
		     : "cc", "memory");
}

static int gcc_inline
sys_getc(void)
{
	int c;

	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYSCALL_GETC),
		       "b" (&c)
		     : "cc", "memory");

	return c;
}

static int gcc_inline
sys_ncpu(void)
{
	int n;

	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYSCALL_NCPU),
		       "b" (&n)
		     : "cc", "memory");

	return n;
}

static int gcc_inline
sys_cpustat(int cpu)
{
	int stat = cpu;

	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYSCALL_CPUSTAT),
		       "b" (&stat)
		     : "cc", "memory");

	return stat;
}

static pid_t gcc_inline
sys_load(uintptr_t binary, pid_t *pid)
{
	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYSCALL_LOAD),
		       "b" (binary),
		       "c" (pid)
		     : "cc", "memory");

	return *pid;
}

static void gcc_inline
sys_mgmt(mgmt_data_t *data)
{
	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYSCALL_MGMT),
		       "b" (data)
		     : "cc", "memory");

	return;
}

static void gcc_inline
sys_startupvm(void)
{
	int ret;
	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYSCALL_STARTUPVM),
		       "b" (&ret)
		     : "cc", "memory");
}

#endif /* !_USER_SYSCALL_H_ */
