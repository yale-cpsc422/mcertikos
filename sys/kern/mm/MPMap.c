#include "MPTNew.h"

#define PAGESIZE	4096
#define PTE_P		0x001	/* Present */
#define PTE_W		0x002	/* Writeable */
#define PTE_U		0x004	/* User-accessible */

void
pt_resv2(unsigned int proc_index1, unsigned int vaddr1,
	unsigned int proc_index2, unsigned int vaddr2)
{
	unsigned int paddr_index;
	paddr_index = palloc();
	//KERN_DEBUG("pt_insert(%u, %u, %u, ...)\n", proc_index1, vaddr1, paddr_index * PAGESIZE);
	pt_insert(proc_index1, vaddr1, paddr_index * PAGESIZE,
		PTE_P | PTE_U | PTE_W);
	//KERN_DEBUG("pt_insert(%u, %u, %u, ...)\n", proc_index2, vaddr2, paddr_index * PAGESIZE);
	pt_insert(proc_index2, vaddr2, paddr_index * PAGESIZE,
		PTE_P | PTE_U | PTE_W);
}


#define VM_USERHI	0xf0000000
#define VM_USERLO	0x40000000

extern void *memcpy(void *dst, const void *src, unsigned int len);
extern void *memset(void *v, unsigned int c, unsigned int n);

unsigned int
pt_copyin(unsigned int pmap_id, unsigned int uva, char *kva, unsigned int len)
{
	if (!(VM_USERLO <= uva && uva + len <= VM_USERHI))
		return 0;

	if ((unsigned int) kva + len > VM_USERHI)
		return 0;

	unsigned int copied = 0;

	while (len) {
		unsigned int uva_pa = pt_read(pmap_id, uva);

		if ((uva_pa & PTE_P) == 0) {
			pt_resv(pmap_id, uva, PTE_P | PTE_U | PTE_W);
			uva_pa = pt_read(pmap_id, uva);
		}

		uva_pa = (uva_pa & 0xfffff000) + (uva % PAGESIZE);

		unsigned int size = (len < PAGESIZE - uva_pa % PAGESIZE) ?
			len : PAGESIZE - uva_pa % PAGESIZE;

		memcpy(kva, (void *) uva_pa, size);

		len -= size;
		uva += size;
		kva += size;
		copied += size;
	}

	return copied;
}

unsigned int
pt_copyout(char *kva, unsigned int pmap_id, unsigned int uva, unsigned int len)
{
	if (!(VM_USERLO <= uva && uva + len <= VM_USERHI))
		return 0;

	if ((unsigned int) kva + len > VM_USERHI)
		return 0;

	unsigned int copied = 0;

	while (len) {
		unsigned int uva_pa = pt_read(pmap_id, uva);

		if ((uva_pa & PTE_P) == 0) {
			pt_resv(pmap_id, uva, PTE_P | PTE_U | PTE_W);
			uva_pa = pt_read(pmap_id, uva);
		}

		uva_pa = (uva_pa & 0xfffff000) + (uva % PAGESIZE);

		unsigned int size = (len < PAGESIZE - uva_pa % PAGESIZE) ?
			len : PAGESIZE - uva_pa % PAGESIZE;

		memcpy((void *) uva_pa, kva, size);

		len -= size;
		uva += size;
		kva += size;
		copied += size;
	}

	return copied;
}

unsigned int
pt_memset(unsigned int pmap_id, unsigned int va, char c, unsigned int len)
{
	unsigned int set = 0;

	while (len) {
		unsigned int pa = pt_read(pmap_id, va);

		if ((pa & PTE_P) == 0) {
			pt_resv(pmap_id, va, PTE_P | PTE_U | PTE_W);
			pa = pt_read(pmap_id, va);
		}

		pa = (pa & 0xfffff000) + (va % PAGESIZE);

		unsigned int size = (len < PAGESIZE - pa % PAGESIZE) ?
			len : PAGESIZE - pa % PAGESIZE;

		memset((void *) pa, c, size);

		len -= size;
		va += size;
		set += size;
	}

	return set;
}
