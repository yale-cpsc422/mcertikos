#include "ring0proc.h"

void
ring0_proc1(void)
{
    dprintf("Ring0 process 1 started.\n");
    while (1)
    {
        dprintf("Ring0 process 1 yielding.\n");
        thread_yield();
        dprintf("Ring0 process 1 resumed.\n");
        //break;
    }
    while (1)
    {
        thread_yield();
    }
}


