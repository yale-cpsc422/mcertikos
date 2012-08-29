#include <proc.h>
#include <stdio.h>
#include <syscall.h>

int
main(int argc, char **argv)
{
	pid_t pid = getpid();
	int pchid = sys_getpchid();

	char buf[128];
	size_t size;

	printf("Receiver (pid %d): parent channel id %d.\n", pid, pchid);

	sys_send(pchid, &pid, sizeof(pid));
	sys_recv(pchid, buf, &size);

	return 0;
}
