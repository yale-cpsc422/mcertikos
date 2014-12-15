#include <proc.h>
#include <stdio.h>
#include <syscall.h>

int
main(int argc, char **argv)
{
	unsigned int balls[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };

  printf("PINGSBUF: %08x\n", balls);

  unsigned int actualsent = 10;
  unsigned int status =
      sys_ssend(3, (uintptr_t)balls, 9, (uintptr_t)&actualsent);

  if (status == E_IPC)
    printf("Bad thing happend in ping.\n");
  else if (status == E_INVAL_PID)
    printf("Trying to send balls to a dead process.\n");

  printf("Ping actually sent %d balls.\n", actualsent);
  printf("Status: %d\n", status);

  /*
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
  */
	return 0;
}
