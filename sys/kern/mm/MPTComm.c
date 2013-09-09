#include "MPTOp.h"

#define PgSize		4096
#define kern_low	262144UL
#define kern_high	983040UL
#define adr_low		(kern_low * PgSize)
#define adr_high	(kern_high * PgSize)
#define PTK_true	259
#define num_proc	64
#define one_k		1024

void
pt_init_comm(unsigned int mbi_addr)
{
	int i;
	unsigned int j;
	mem_init(mbi_addr);
	i = 0;
	while (i < num_proc) {
		j = 0;
		while (j < one_k) {
			set_PDX(i, j);
			j++;
		}
		j = 0;
		while (j < 0xffffffff) {
			if (j < adr_low || j >= adr_high)
				pt_insert(i, j, j, PTK_true);
			else
				pt_unpresent(i, j);

			if (j == 0xfffff000)
				break;

			j += PgSize;
		}
		i++;
	}
}
