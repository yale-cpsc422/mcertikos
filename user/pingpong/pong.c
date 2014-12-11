#include <proc.h>
#include <stdio.h>
#include <syscall.h>

int
main(int argc, char **argv)
{
	uint32_t ball = 0;

	while (1) 
  {
		sys_srecv(2, &ball);
		printf("pong %d!\n", ball);
		yield();
	}
	return 0;
}
