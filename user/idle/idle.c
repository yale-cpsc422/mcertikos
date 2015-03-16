#include <proc.h>
#include <stdio.h>
#include <syscall.h>

#define NUM_PROC 64

int
main(int argc, char **argv)
{
	printf("idle\n");

    /*
	pid_t ping_pid, pong_pid, ring0_id1, ring0_id2;

	if ((ping_pid = spawn(1)) != NUM_PROC)
		printf("ping in process %d.\n", ping_pid);
	else
		printf("Failed to launch ping.\n");

	if ((pong_pid = spawn(2)) != NUM_PROC)
		printf("pong in process %d.\n", pong_pid);
	else
		printf("Failed to launch pong.\n");

    if ((ring0_id1 = sys_ring0_spawn(1)) != NUM_PROC)
        printf("The first ring0 process in process %d.\n", ring0_id1);
	else
		printf("Failed to launch the first ring0 process.\n");

    if ((ring0_id2 = sys_ring0_spawn(2)) != NUM_PROC)
        printf("The second ring0 process in process %d.\n", ring0_id2); 
	else
		printf("Failed to launch the second ring0 process.\n");
    */

	pid_t vmm_pid;

	printf("try to launch VMM.\n");

	if ((vmm_pid = spawn(0)) != -1)
		printf("VMM in process %d.\n", vmm_pid);
	else
		printf("Failed to launch VMM.\n");

	while (1)
		yield();

	return 0;
}
