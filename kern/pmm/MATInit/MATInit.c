#include <lib/debug.h>
#include "import.h"

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
	devinit(mbi_addr);
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


