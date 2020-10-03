#include <proc.h>
#include <stdio.h>
#include <syscall.h>

int main(int argc, char **argv)
{
    unsigned int val = 100;
    unsigned int *addr = (unsigned int *) 0xe0000000;

    printf("ping started.\n");
    printf("ping: the value at address %x: %d\n", addr, *addr);
    printf("ping: writing the value %d to the address %x\n", val, addr);
    *addr = val;
    yield();
    printf("ping: the new value at address %x: %d\n", addr, *addr);

    return 0;
}
