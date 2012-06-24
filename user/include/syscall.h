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
sys_spawn(uintptr_t exe)
{
	pid_t pid;

	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_spawn),
		       "b" (exe),
		       "c" (&pid)
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

static gcc_inline void
sys_send(pid_t pid, void *data, size_t size)
{
	if (size != 0 && data == NULL)
		return;

	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_send),
		       "b" (pid),
		       "c" (data),
		       "d" (size)
		     : "cc", "memory");
}

static gcc_inline void
sys_recv(void *data, size_t *size)
{
	if (data == NULL || size  == NULL)
		return;

	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_recv),
		       "b" (data),
		       "c" (size)
		     : "cc", "memory");
}

#endif /* !_USER_SYSCALL_H_ */
