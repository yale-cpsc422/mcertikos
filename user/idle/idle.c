#include <proc.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
	printf("idle\n");

	while (1)
		yield();

	return 0;
}
