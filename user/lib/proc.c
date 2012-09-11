#include <gcc.h>
#include <proc.h>
#include <string.h>
#include <syscall.h>
#include <types.h>

gcc_inline pid_t
spawn(uint32_t cpu_idx, uintptr_t exec)
{
	return sys_spawn(cpu_idx, exec);
}

gcc_inline void
yield(void)
{
	sys_yield();
}

gcc_inline pid_t
getpid(void)
{
	return sys_getpid();
}

gcc_inline int
getchid(pid_t pid)
{
	int chid;

	if (sys_getchid(pid, &chid))
		return -1;
	else
		return chid;
}
