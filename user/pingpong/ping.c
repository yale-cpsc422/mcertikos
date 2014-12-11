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
		if (sys_ssend(3, ball++) == E_IPC)
      printf("Error occured in ping\n");
		sys_yield();
	}
	return 0;
}
