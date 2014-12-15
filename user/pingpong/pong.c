#include <proc.h>
#include <stdio.h>
#include <syscall.h>

int
main(int argc, char **argv)
{

  unsigned int receivebuffer[32];
  unsigned int actualreceived;

  printf("PONGRBUF = %08x\n", receivebuffer);

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


  /*
	while (1) 
  {
		sys_srecv(2, &ball);
		printf("pong %d!\n", ball);
		sys_yield();
	}
  */
	return 0;
}
