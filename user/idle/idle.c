#include <proc.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
	printf("idle %d.\n", getpid());

	while (1)
		asm volatile("pause" ::: "memory");

	return 0;
}
