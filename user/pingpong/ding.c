#include <proc.h>
#include <stdio.h>
#include <syscall.h>

int
main(int argc, char **argv)
{
  unsigned int balls[] = { 0, 1, 2 };
  unsigned int actualsent;
  unsigned int scount = 3;
  printf("Ding sent 3 balls to pong.\n");
  unsigned int status = sys_ssend(3, balls, scount, &actualsent);

  if (status == E_IPC)
    printf("Bad thing happend in ding.\n");
  else if (status == E_INVAL_PID)
    printf("Trying to send balls to a dead process.\n");

  printf("Ding actually sent %d balls.\n", actualsent);
  //printf("DING\n");
  return 0;
}
