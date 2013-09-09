static int NPS_LOC;

struct A {
	int isnorm;
	int allocated;
};

static struct A AT_LOC[1 << 20];

int
get_nps()
{
	return NPS_LOC;
}

void
set_nps(int nps)
{
	NPS_LOC = nps;
}

int
is_norm(int t_is_norm_index)
{
	int tisnorm;

	tisnorm = AT_LOC[t_is_norm_index].isnorm;

	if (tisnorm == 0) {
		tisnorm = 0;
	} else {
		if (tisnorm == 1)
			tisnorm = 0;
		else
			tisnorm = 1;
	}

	return tisnorm;
}

void
set_norm(int set_norm_index, int norm_val)
{
	AT_LOC[set_norm_index].isnorm = norm_val;
	AT_LOC[set_norm_index].allocated = 0;
}

int
at_get(int at_get_index)
{
	int allocated;

	allocated = AT_LOC[at_get_index].allocated;
	if (allocated == 0)
		allocated = 0;
	else
		allocated = 1;

	return allocated;
}

void
at_set(int at_set_index, int allocated_val)
{
	AT_LOC[at_set_index].allocated = allocated_val;
}
