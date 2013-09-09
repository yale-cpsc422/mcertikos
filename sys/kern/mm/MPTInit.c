#include "MPTKern.h"

void
pt_init(unsigned int mbi_addr)
{
	pt_init_kern(mbi_addr);
	set_PT(0);
	set_pe();
}
