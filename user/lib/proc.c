#include <proc.h>
#include <syscall.h>
#include <types.h>

pid_t
spawn(uint32_t cpu_idx, uintptr_t exec)
{
	pid_t new_p = sys_create_proc(exec);
	if (new_p == -1)
		return -1;
	if (sys_run_proc(new_p, cpu_idx))
		return -1;
	return new_p;
}

void
yield(void)
{
	sys_yield();
}

pid_t
getpid(void)
{
	return sys_getpid();
}

pid_t
getppid(void)
{
	return sys_getppid();
}
