#include "MPTBit.h"

#define PAGESIZE	4096
#define VM_USERLO	0x40000000
#define VM_USERHI	0xF0000000
#define VM_USERLO_PI	(VM_USERLO / PAGESIZE)
#define VM_USERHI_PI	(VM_USERHI / PAGESIZE)

#define NUM_PROC	64

void
pmap_init(unsigned int mbi_addr)
{
	unsigned int i;
	pt_init(mbi_addr);
	set_bit(0, 1);
	i = 1;
	while (i < NUM_PROC) {
		set_bit(i, 0);
		i++;
	}
}

unsigned int
pt_new(void)
{
	unsigned int pt_new_index;
	unsigned int pt_new_is_used;
	unsigned int pt_new_free_index;
	pt_new_index = 0;
	pt_new_free_index = NUM_PROC;
	while (pt_new_index < NUM_PROC && pt_new_free_index == NUM_PROC) {
		pt_new_is_used = is_used(pt_new_index);
		if (pt_new_is_used != 1)
			pt_new_free_index = pt_new_index;
		pt_new_index ++;
	}
	set_bit(pt_new_free_index, 1);
	return pt_new_free_index;
}

void
pt_free(unsigned int pid)
{
	unsigned int j;
	set_bit(pid, 0);
	j = VM_USERLO;
	while (j < VM_USERHI) {
		pt_rmv(pid, j);
		j = j + PAGESIZE;
	}
}
