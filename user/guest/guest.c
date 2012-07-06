#include <proc.h>
#include <stdio.h>
#include <syscall.h>

int
main(int argc, char **argv)
{
	pid_t pid = getpid();

	printf("guest %d: create VM ... ", pid);
	if (sys_allocvm()) {
		printf("failed.\n");
		return 1;
	}

	printf("done.\n");

	printf("guest %d: start VM ... ", pid);
	sys_execvm();
	printf("done.\n");

	return 0;
}
