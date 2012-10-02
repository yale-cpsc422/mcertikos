#include <proc.h>
#include <syscall.h>
#include <types.h>

pid_t
spawn(uint32_t cpu_idx, sid_t sid, uintptr_t exec)
{
	return sys_spawn(cpu_idx, sid, exec);
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
