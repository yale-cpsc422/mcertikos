#include <preinit/lib/debug.h>
#include <preinit/lib/types.h>
#include <kern/mm/MALOp.h>

#define NUM_CHAN		64
#define TD_STATE_RUN		1

static void
kern_main (void)
{
    KERN_DEBUG("In kernel main.\n");
}

void
kern_init (uintptr_t mbi_addr)
{
    mem_init (mbi_addr);

    KERN_DEBUG("Kernel initialized.\n");

    kern_main ();
}
