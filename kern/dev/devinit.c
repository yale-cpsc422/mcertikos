#include <lib/x86.h>
#include <lib/types.h>
#include <lib/debug.h>

#include "console.h"
#include "mboot.h"

void
devinit (uintptr_t mbi_addr)
{
    enable_sse ();

    cons_init ();
    KERN_DEBUG("cons initialized.\n");

    pmmap_init (mbi_addr);
}
