#include "at.h"

int
palloc(void)
{
	int nps, idx, cur_at, is_norm, free_idx;

	nps = at_get_nps();
	idx = 0;
	free_idx = nps;

	while (idx < nps && free_idx == nps) {
		is_norm = at_is_norm(idx);

		if (is_norm == 1) {
			cur_at = at_get(idx);
			if (cur_at == 0)
				free_idx = idx;
		}

		idx++;
	}

	if (free_idx != nps) {
		at_set(free_idx, 1);
		return free_idx;
	}

	return -1;
}

void
pfree(int idx)
{
	at_set(idx, 0);
}
