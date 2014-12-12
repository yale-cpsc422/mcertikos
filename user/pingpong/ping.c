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
    unsigned int status = sys_ssend(3, ball++);
		if (status == E_IPC) {
      printf("Error occured in ping\n");
      break;
    } else if (status == E_INVAL_PID) {
      printf("Trying to send to a dead process in ping\n");
      break;
    } else if (status != E_SUCC) {
      printf("Unknown error in ping: %d\n", status);
      break;
    }
		sys_yield();
	}
	return 0;
}
