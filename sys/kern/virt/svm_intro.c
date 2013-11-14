#define XVMST_SIZE		6

unsigned int XVMST_LOC[XVMST_SIZE];

unsigned int
xvmst_read(unsigned int ofs)
{
	return XVMST_LOC[ofs];
}

void
xvmst_write(unsigned int ofs, unsigned int v)
{
	XVMST_LOC[ofs] = v;
}
