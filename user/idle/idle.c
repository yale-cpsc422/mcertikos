#include <proc.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
	printf("idle\n");

#if 0
	pid_t ping_pid, pong_pid;

	if ((ping_pid = spawn(1)) != -1)
		printf("ping in process %d.\n", ping_pid);
	else
		printf("Failed to launch ping.\n");

	if ((pong_pid = spawn(2)) != -1)
		printf("pong in process %d.\n", pong_pid);
	else
		printf("Failed to launch pong.\n");
#else
	pid_t vmm_pid;

	if ((vmm_pid = spawn(0)) != -1)
		printf("VMM in process %d.\n", vmm_pid);
	else
		printf("Failed to launch VMM.\n");
#endif

	while (1)
		yield();

	return 0;
}
