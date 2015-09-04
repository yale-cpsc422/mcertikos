#include <preinit/dev/console.h>

#include <preinit/lib/mboot.h>
#include <preinit/lib/x86.h>
#include <preinit/lib/types.h>

#include <preinit/lib/debug.h>


void
preinit (uintptr_t mbi_addr)
{
    enable_sse ();

    cons_init ();
    KERN_DEBUG("cons initialized.\n");

    pmmap_init (mbi_addr);
}
