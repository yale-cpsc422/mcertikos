static int PTP_LOC[64];

void
set_bit(int proc_index, int val)
{
	PTP_LOC[proc_index] = val;
}

int
is_used(int proc_index)
{
	int val;
	val = PTP_LOC[proc_index];
	return val;
}
