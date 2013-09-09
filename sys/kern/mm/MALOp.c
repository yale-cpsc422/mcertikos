#include "MALInit.h"

void
mem_init(unsigned int mbi_addr)
{
	int i, j, isnorm, nps, maxs, size, flag;
	unsigned int s, l;
	pmmap_init(mbi_addr);
	i = 0;
	size = pmmap_entries_nr();
	nps = 0;
	while (i < size) {
		s = pmmap_entry_start(i);
		l = pmmap_entry_length(i);
		maxs = (s + l) / 4096 + 1;
		if (maxs > nps)
			nps = maxs;
		i++;
	}
	set_nps(nps);
	i = 0;
	while (i < nps) {
		if (i < 262144 || i >= 983040) {
			set_norm(i, 1);
		} else {
			j = 0;
			flag = 0;
			isnorm = 0;
			while (j < size && flag == 0) {
				s = pmmap_entry_start(j);
				l = pmmap_entry_length(j);
				isnorm = pmmap_entry_usable(j);
				if (s <= i * 4096 && l + s >= (i + 1) * 4096) {
					flag = 1;
				}
				j++;
			}
			if (flag == 1 && isnorm == 1)
				set_norm(i, 2);
			else
				set_norm(i, 0);
		}
		i++;
	}
}
