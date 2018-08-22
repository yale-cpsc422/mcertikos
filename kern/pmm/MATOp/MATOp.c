#include <lib/debug.h>
#include "import.h"

static unsigned int last_palloc_index = 0;

/**
 * Allocate a physical page.
 *
 * 1. First, implement a naive page allocator that scans the allocation table (AT)
 *    using the functions defined in import.h to find the first unallocated page
 *    with usable permission.
 *    (Q: Do you have to scan the allocation table from index 0? Recall how you have
 *    initialized the table in pmem_init.)
 *    Then mark the page as allocated in the allocation table and return the page
 *    index of the page found. In the case when there is no available page found,
 *    return 0.
 * 2. Optimize the code using memoization so that you do not have to
 *    scan the allocation table from scratch every time.
 */
unsigned int palloc()
{
    unsigned int nps;
    unsigned int palloc_index;
    unsigned int palloc_free_index;

    nps = get_nps();
    palloc_index = last_palloc_index + 1;
    palloc_free_index = nps;

    while (palloc_index < nps && palloc_free_index == nps) {
        if (at_is_norm(palloc_index)) {
            if (!at_is_allocated(palloc_index)) {
                palloc_free_index = palloc_index;
            }
        }
        palloc_index++;
    }

    if (palloc_free_index == nps) {
        palloc_free_index = 0;
    } else {
        at_set_allocated(palloc_free_index, 1);
    }

    last_palloc_index = palloc_free_index;
    return palloc_free_index;
}

/**
 * Free a physical page.
 *
 * This function marks the page with given index as unallocated
 * in the allocation table.
 *
 * Hint: Simple.
 */
void pfree(unsigned int pfree_index)
{
    at_set_allocated(pfree_index, 0);
}
