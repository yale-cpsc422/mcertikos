#include <proc.h>
#include <stdio.h>
#include <syscall.h>

int
main(int argc, char **argv)
{
  unsigned int balls[] = { 0, 1, 2 };
  unsigned int actualsent;
  unsigned int scount = 3;
  sys_ssend(3, balls, scount, &actualsent);
  printf("DING\n");
  return 0;
}
