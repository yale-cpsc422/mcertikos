#include "MPTBit.h"
#include <preinit/lib/debug.h>

#define NUM_PROC	64
#define MagicNumber 1048577

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
pt_resv(unsigned int proc_index, unsigned int vaddr, unsigned int perm)
{
    unsigned int pi;
    unsigned int result;
    pi = palloc();
    if (pi == 0)
      result = MagicNumber;
    else
      result = pt_insert(proc_index, vaddr, pi, perm);
    KERN_DEBUG("In pt_resv: proc_index = %u, vaddr = %u, perm = %u, pi = %u, result = %u.\n", proc_index, vaddr, perm, pi, result);
    return result;
}   

unsigned int
pt_resv2(unsigned int proc_index, unsigned int vaddr, unsigned int perm, unsigned int proc_index2, unsigned int vaddr2, unsigned int perm2)
{   
    unsigned int pi;
    unsigned int result;
    pi = palloc();
    if (pi == 0)
      result = MagicNumber;
    else
    {
      result = pt_insert(proc_index, vaddr, pi, perm);
      if (result != MagicNumber)
        result = pt_insert(proc_index2, vaddr2, pi, perm2);
    }
    return result;
}

unsigned int
pt_new(void)
{
	unsigned int pt_new_index;
	unsigned int pt_new_is_used;
	unsigned int pt_new_free_index;
	pt_new_index = 1;
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

