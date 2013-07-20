#include <proc.h>
#include <syscall.h>
#include <types.h>

pid_t
spawn(uintptr_t exec)
{
	return sys_spawn(exec);
}

void
yield(void)
{
	sys_yield();
}
