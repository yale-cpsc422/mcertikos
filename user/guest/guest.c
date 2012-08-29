#include <proc.h>
#include <stdio.h>
#include <syscall.h>

int
main(int argc, char **argv)
{
#if 1
	extern uint8_t _binary___obj_user_test_receiver_recv_start[];
	pid_t receiver[4];
	int i;

	for (i = 0; i < 4; i++) {
		receiver[i] =
			spawn(1, (uintptr_t)
			      _binary___obj_user_test_receiver_recv_start);
		printf("Spawn receiver%d (pid %d).\n", i, receiver[i]);
	}

	return 0;
#else
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
#endif

	return 0;
}
