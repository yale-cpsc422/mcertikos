#include "ring0proc.h"

void
ring0_proc2(void)
{
    dprintf("Ring0 process 2 started.\n");
    while (1)
    {
        dprintf("Ring0 process 2 yielding.\n");
        thread_yield();
        dprintf("Ring0 process 2 resumed.\n");
    }
}


