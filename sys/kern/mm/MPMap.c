#include "MPTNew.h"

#define PgSize		4096

#define PTE_P		0x001	/* Present */
#define PTE_W		0x002	/* Writeable */
#define PTE_U		0x004	/* User-accessible */

#define VM_USERHI	0xf0000000
#define VM_USERLO	0x40000000

extern void *memcpy(void *dst, const void *src, unsigned int len);
extern void *memset(void *v, int c, unsigned int n);

void
pt_resv(int proc_index, unsigned int vaddr, int perm)
{
	int paddr_index;
	paddr_index = palloc();
	pt_insert(proc_index, vaddr, paddr_index * PgSize, perm);
}

int
pt_copyin(int pmap_id, unsigned int uva, char *kva, unsigned int len)
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

		uva_pa = (uva_pa & 0xfffff000) + (uva % PgSize);

		unsigned int size = (len < PgSize - uva_pa % PgSize) ?
			len : PgSize - uva_pa % PgSize;

		memcpy(kva, (void *) uva_pa, size);

		len -= size;
		uva += size;
		kva += size;
		copied += size;
	}

	return copied;
}

int
pt_copyout(char *kva, int pmap_id, unsigned int uva, unsigned int len)
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

		uva_pa = (uva_pa & 0xfffff000) + (uva % PgSize);

		unsigned int size = (len < PgSize - uva_pa % PgSize) ?
			len : PgSize - uva_pa % PgSize;

		/* KERN_DEBUG("Copy %d bytes from KVA 0x%08x to PA 0x%08x.\n", */
		/* 	   size, kva, uva_pa); */

		memcpy((void *) uva_pa, kva, size);

		len -= size;
		uva += size;
		kva += size;
		copied += size;
	}

	return copied;
}

int
pt_memset(int pmap_id, unsigned int va, char c, unsigned int len)
{
	unsigned int set = 0;

	while (len) {
		unsigned int pa = pt_read(pmap_id, va);

		if ((pa & PTE_P) == 0) {
			pt_resv(pmap_id, va, PTE_P | PTE_U | PTE_W);
			pa = pt_read(pmap_id, va);
		}

		pa = (pa & 0xfffff000) + (va % PgSize);

		unsigned int size = (len < PgSize - pa % PgSize) ?
			len : PgSize - pa % PgSize;

		memset((void *) pa, c, size);

		len -= size;
		va += size;
		set += size;
	}

	return set;
}
