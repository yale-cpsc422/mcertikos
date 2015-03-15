#include <proc.h>
#include <stdio.h>
#include <syscall.h>

int
main(int argc, char **argv)
{
	int ball = 1;

	while (1) 
  {
		printf("ping %d!\n", ball);
		if (sys_send(3, ball++) == E_IPC)
			sys_sleep(3);
		//yield();
    break;
	}
	return 0;
}
