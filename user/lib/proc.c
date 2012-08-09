#include <gcc.h>
#include <proc.h>
#include <string.h>
#include <syscall.h>
#include <types.h>

gcc_inline pid_t
spawn(uintptr_t exec)
{
	return sys_spawn(exec);
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

gcc_inline size_t
recv(struct msg *msg)
{
	size_t size;

	sys_recv(msg, &size);

	return size - sizeof(pid_t);
}

void
send(pid_t pid, void *buf, size_t size)
{
	struct msg msg;

	msg.pid = getpid();
	memcpy(&msg.data, buf, size);

	sys_send(pid, &msg, size + sizeof(pid_t));
}
