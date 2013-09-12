#include "MPTBit.h"

#define num_proc	64
#define PgSize		4096
#define kern_low	262144UL
#define kern_high	983040UL
#define adr_low		(kern_low * PgSize)
#define adr_high	(kern_high * PgSize)

void
pmap_init(unsigned int mbi_addr)
{
	int i;
	pt_init(mbi_addr);
	set_bit(0, 1);
	i = 1;
	while (i < num_proc) {
		set_bit(i, 0);
		i++;
	}
}

int
pt_new(void)
{
	int pt_new_index;
	int pt_new_is_used;
	int pt_new_free_index;
	pt_new_index = 0;
	pt_new_free_index = num_proc;
	while (pt_new_index < num_proc && pt_new_free_index == num_proc) {
		pt_new_is_used = is_used(pt_new_index);
		if (pt_new_is_used != 1)
			pt_new_free_index = pt_new_index;
		pt_new_index ++;
	}
	set_bit(pt_new_free_index, 1);
	return pt_new_free_index;
}

void
pt_free(int proc_index)
{
	unsigned int j;
	set_bit(proc_index, 0);
	j = adr_low;
	while (j < adr_high) {
		pt_rmv(proc_index, j);
		j = j + PgSize;
	}
}
