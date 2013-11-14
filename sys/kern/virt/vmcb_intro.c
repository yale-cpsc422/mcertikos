#include <lib/gcc.h>

#define VMCB_SIZE		4096

unsigned int VMCB_LOC[VMCB_SIZE] gcc_aligned(VMCB_SIZE);

unsigned int
vmcb_read_z(unsigned int ofs)
{
	return VMCB_LOC[ofs];
}

void
vmcb_write_z(unsigned int ofs, unsigned int v)
{
	VMCB_LOC[ofs] = v;
}


unsigned int
vmcb_read_v(unsigned int ofs)
{
	return VMCB_LOC[ofs];
}

void
vmcb_write_v(unsigned int ofs, unsigned int v)
{
	VMCB_LOC[ofs] = v;
}
