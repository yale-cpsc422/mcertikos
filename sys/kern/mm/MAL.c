#include <preinit/lib/debug.h>

#include "MALOp.h"

void
pfree(int pfree_index)
{
	at_set(pfree_index, 0);
}

int
palloc(void)
{
	int tnps;
	int palloc_index;
	int palloc_cur_at;
	int palloc_is_norm;
	int palloc_free_index;
	tnps = get_nps();
	palloc_index = 0;
	palloc_free_index = tnps;
	while (palloc_index < tnps && palloc_free_index == tnps) {
		palloc_is_norm = is_norm(palloc_index);
		if (palloc_is_norm == 1) {
			palloc_cur_at = at_get(palloc_index);
			if (palloc_cur_at == 0)
				palloc_free_index = palloc_index;
		}
		palloc_index ++;
	}

	if (palloc_index == tnps)
		KERN_PANIC("Memory is used out.\n");

	at_set(palloc_index - 1, 1);
	return palloc_index;
}
