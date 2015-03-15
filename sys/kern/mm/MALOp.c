#include <preinit/lib/debug.h>
#include "MALInit.h"

#define PAGESIZE	4096
#define VM_USERLO	0x40000000
#define VM_USERHI	0xF0000000
#define VM_USERLO_PI	(VM_USERLO / PAGESIZE)
#define VM_USERHI_PI	(VM_USERHI / PAGESIZE)

void
mem_init(unsigned int mbi_addr)
{
	unsigned int i, j, isnorm, nps, maxs, size, flag;
	unsigned int s, l;
	preinit(mbi_addr);
	i = 0;
	size = get_size();
	nps = 0;
	while (i < size) {
		s = get_mms(i);
		l = get_mml(i);
		maxs = (s + l) / PAGESIZE + 1;
		if (maxs > nps)
			nps = maxs;
		i++;
	}
	set_nps(nps);
	i = 0;
	while (i < nps) {
		if (i < VM_USERLO_PI || i >= VM_USERHI_PI) {
			set_norm(i, 1);
		} else {
			j = 0;
			flag = 0;
			isnorm = 0;
			while (j < size && flag == 0) {
				s = get_mms(j);
				l = get_mml(j);
				isnorm = is_usable(j);
				if (s <= i * PAGESIZE && l + s >= (i + 1) * PAGESIZE) {
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

void
pfree(unsigned int pfree_index)
{
	at_set(pfree_index, 0);
}

unsigned int
palloc()
{
    unsigned int tnps;
    unsigned int palloc_index;
    unsigned int palloc_cur_at;
    unsigned int palloc_is_norm;
    unsigned int palloc_free_index;
    tnps = get_nps();
    palloc_index = 1;
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
      at_set_c(palloc_free_index, 0);
    }
    return palloc_free_index;
} 

