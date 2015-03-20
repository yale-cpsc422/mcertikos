#include <proc.h>
#include <stdio.h>
#include <syscall.h>

int
main(int argc, char **argv)
{
	unsigned int balls[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };

  unsigned int actualsent;

  printf("Ping sent 9 balls to pong.\n");
  unsigned int status =
      sys_ssend(3, balls, 9, &actualsent);

  if (status == E_IPC)
    printf("Bad thing happend in ping.\n");
  else if (status == E_INVAL_PID)
    printf("Trying to send balls to a dead process.\n");

  printf("Ping actually sent %d balls.\n", actualsent);

  //printf("ping.\n");

  /*
  while(1) {
    //printf("ping yielding\n");
    yield();
  }
  */

	return 0;
}
