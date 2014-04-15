#include "ring0proc.h"

void
ring0_proc1(void)
{
    while (1)
    {
        dprintf("In ring0 process 1...\n");
        thread_yield();
    }
}


