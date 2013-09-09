#include "MPTComm.h"

#define PgSize		4096
#define kern_low	262144UL
#define kern_high	983040UL
#define adr_low		(kern_low * PgSize)
#define adr_high	(kern_high * PgSize)
#define PTK_false	3

void
pt_init_kern(unsigned int mbi_addr)
{
	unsigned int i;
	pt_init_comm(mbi_addr);
	i = adr_low;
	while (i < adr_high) {
		pt_insert(0, i, i, PTK_false);
		i = i + PgSize;
	}
}
