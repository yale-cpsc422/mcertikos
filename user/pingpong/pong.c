#include <proc.h>
#include <stdio.h>
#include <syscall.h>

int
main(int argc, char **argv)
{

  unsigned int receivebuffer[32];
  unsigned int actualreceived;

  unsigned int status =
    sys_srecv(2, receivebuffer, 32, &actualreceived);

  if (status == E_IPC)
    printf("Bad thing happened in pong.\n");
  else if (status == E_INVAL_PID)
    printf("Trying to receive from dead process.\n");

  printf("Pong received %d balls.\n", actualreceived);
  printf("Status: %d\n", status);

  unsigned int i;
  for (i = 0; i < actualreceived; i++) {
    printf("ball[%d] = %d\n", i, receivebuffer[i]);
  }

  sys_srecv(4, receivebuffer, 32, &actualreceived);
  for (i = 0; i < actualreceived; i++) {
    printf("ball[%d] = %d\n", i, receivebuffer[i]);
  }

  //printf("pong.\n");

  /*
  while(1) {
    //printf("pong yielding\n");
    yield();
  }
  */

	return 0;
}
