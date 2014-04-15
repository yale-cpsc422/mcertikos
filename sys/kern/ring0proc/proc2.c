#include "ring0proc.h"

void
ring0_proc2(void)
{
    while (1)
    {
        dprintf("In ring0 process 2...\n");
        thread_yield();
    }
}


