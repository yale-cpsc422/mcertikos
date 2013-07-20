#include "at.h"

struct pageinfo {
	int isnorm;
	int allocated;
};

static struct pageinfo	alloc_table[1<<20];
static int		nps;

int
at_get_nps(void)
{
	return nps;
}

void
at_set_nps(int v)
{
	nps = v;
}

int
at_is_norm(int idx)
{
	if (alloc_table[idx].isnorm == 0 || alloc_table[idx].isnorm == 1)
		return 0;
	else
		return 1;
}

void
at_set_norm(int idx, int val)
{
	alloc_table[idx].isnorm = val;
	alloc_table[idx].allocated = 0;
}

int
at_get(int idx)
{
	return alloc_table[idx].allocated;
}

void
at_set(int idx, int val)
{
	alloc_table[idx].allocated = val;
}
