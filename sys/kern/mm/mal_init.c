#include <lib/export.h>

#include "at.h"
#include "mal_init.h"

#define PAGESIZE	4096
#define VM_USERHI	0xf0000000
#define VM_USERLO	0x40000000

void
mem_init(void)
{
	int page_idx, pmmap_idx, isnorm, nps, maxs, size, flag;
	uintptr_t start, length;

	pmmap_idx = 0;
	size = pmmap_entries_nr();
	nps = 0;

	while (pmmap_idx < size) {
		start = pmmap_entry_start(pmmap_idx);
		length = pmmap_entry_length(pmmap_idx);
		maxs = (start + length) / PAGESIZE + 1;
		if (maxs > nps)
			nps = maxs;
		pmmap_idx++;
	}

	at_set_nps(nps);

	page_idx = 0;

	while (page_idx < nps) {
		if (page_idx < VM_USERLO / PAGESIZE ||
		    page_idx >= VM_USERHI / PAGESIZE) {
			at_set_norm(page_idx, 1);
		} else {
			pmmap_idx = 0;
			flag = 0;
			isnorm = 0;

			while (pmmap_idx < size && flag == 0) {
				start = pmmap_entry_start(pmmap_idx);
				length = pmmap_entry_length(pmmap_idx);
				isnorm = pmmap_entry_usable(pmmap_idx);

				if (start <= page_idx * PAGESIZE &&
				    start + length >= (page_idx + 1) * PAGESIZE)
					flag = 1;

				pmmap_idx++;
			}

			if (flag == 1 && isnorm == 1)
				at_set_norm(page_idx, 2);
			else
				at_set_norm(page_idx, 0);
		}
		page_idx++;
	}
}
