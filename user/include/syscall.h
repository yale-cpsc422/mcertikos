#ifndef _USER_SYSCALL_H_
#define _USER_SYSCALL_H_

#include <sys/syscall.h>

#include <gcc.h>
#include <proc.h>
#include <types.h>

static gcc_inline void
sys_test(uint32_t a)
{
	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_test),
		       "b" (a)
		     : "cc", "memory");
}

static gcc_inline void
sys_puts(const char *s)
{
	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_puts),
		       "b" (s)
		     : "cc", "memory");
}

static gcc_inline int
sys_getc(void)
{
	int c;

	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_getc),
		       "b" (&c)
		     : "cc", "memory");

	return c;
}

static gcc_inline pid_t
sys_spawn(uint32_t cpu_idx, uintptr_t exe)
{
	pid_t pid;

	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_spawn),
		       "b" (cpu_idx),
		       "c" (exe),
		       "d" (&pid)
		     : "cc", "memory");

	return pid;
}

static gcc_inline void
sys_yield(void)
{
	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_yield)
		     : "cc", "memory");
}

static gcc_inline pid_t
sys_getpid(void)
{
	pid_t pid;

	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_getpid),
		       "b" (&pid)
		     : "cc", "memory");

	return pid;
}

static gcc_inline int
sys_allocvm(void)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_allocvm)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_execvm(void)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_execvm)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_ncpus(void)
{
	int n;

	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_ncpus),
		       "b" (&n)
		     : "cc", "memory");

	return n;
}

static gcc_inline int
sys_getpchid(void)
{
	int chid;

	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_getpchid),
		       "b" (&chid)
		     : "cc", "memory");

	return chid;
}

static gcc_inline int
sys_send(int channel_id, void *msg, size_t size)
{
	int errno;

	if (channel_id < 0 || msg == NULL || size == 0)
		return -1;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_send),
		       "b" (channel_id),
		       "c" (msg),
		       "d" (size)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_recv(int channel_id, void *msg, size_t *size)
{
	int errno;

	if (channel_id < 0 || msg == NULL || size == NULL)
		return -1;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_recv),
		       "b" (channel_id),
		       "c" (msg),
		       "d" (size)
		     : "cc", "memory");

	return errno;
}

#endif /* !_USER_SYSCALL_H_ */
