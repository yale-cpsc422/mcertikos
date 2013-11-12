static unsigned int PTP_LOC[64];

void
set_bit(unsigned int pid, unsigned int val)
{
	PTP_LOC[pid] = val;
}

unsigned int
is_used(unsigned int pid)
{
	unsigned int val;
	val = PTP_LOC[pid];
	return val;
}
