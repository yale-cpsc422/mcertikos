#include <proc.h>
#include <stdio.h>
#include <syscall.h>

int
main(int argc, char **argv)
{
	while (1) {
		printf("pong!\n");
		yield();
	}
	return 0;
}
