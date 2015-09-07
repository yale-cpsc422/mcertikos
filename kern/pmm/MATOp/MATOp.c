#include <lib/debug.h>
#include "import.h"

#define PAGESIZE	4096
#define VM_USERLO	0x40000000
#define VM_USERHI	0xF0000000
#define VM_USERLO_PI	(VM_USERLO / PAGESIZE)
#define VM_USERHI_PI	(VM_USERHI / PAGESIZE)

static unsigned int last_palloc_index = 0;

unsigned int
palloc()
{
    unsigned int tnps;
    unsigned int palloc_index;
    unsigned int palloc_cur_at;
    unsigned int palloc_is_norm;
    unsigned int palloc_free_index;
    tnps = get_nps();
    palloc_index = last_palloc_index + 1;
    palloc_free_index = tnps;
    while( palloc_index < tnps && palloc_free_index == tnps )
    {
        palloc_is_norm = is_norm(palloc_index);
        if (palloc_is_norm == 1)
        {
            palloc_cur_at = at_get(palloc_index);
            if (palloc_cur_at == 0)
                palloc_free_index = palloc_index;
        }
        palloc_index ++;
    }
    if (palloc_free_index == tnps)
      palloc_free_index = 0;
    else
    {
      at_set(palloc_free_index, 1);
    }
    last_palloc_index = palloc_free_index;
    return palloc_free_index;
} 

void
pfree(unsigned int pfree_index)
{
	at_set(pfree_index, 0);
}
