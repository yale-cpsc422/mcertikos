#include <sys/debug.h>
#include <sys/elf.h>
#include <sys/mmu.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/proc.h>

#include <machine/pmap.h>

void
elf_load(pmap_t *pmap, uintptr_t exe, proc_t * p)
{
	KERN_ASSERT(pmap != NULL);
	KERN_ASSERT(p->pmap != NULL);
	KERN_DEBUG("pmap_t:%x,processpmap_t:%x \n",pmap,p->pmap);

	elfhdr *eh = (elfhdr *) exe;
	KERN_ASSERT(eh->e_magic == ELF_MAGIC);
	KERN_DEBUG("eh:%x, \n",eh);

	// Load each program segment
	proghdr *ph = (proghdr *) ((void *) eh + eh->e_phoff);
	proghdr *eph = ph + eh->e_phnum;
	for (; ph < eph; ph++) {
		if (ph->p_type != ELF_PROG_LOAD)
			continue;

		void *fa = (void *) eh + ROUNDDOWN(ph->p_offset, PAGESIZE);
		uint32_t va = ROUNDDOWN(ph->p_va, PAGESIZE);
		uint32_t zva = ph->p_va + ph->p_filesz;
		uint32_t eva = ROUNDUP(ph->p_va + ph->p_memsz, PAGESIZE);

		uint32_t perm = PTE_P | PTE_U | PTE_G;
		if (ph->p_flags & ELF_PROG_FLAG_WRITE)
			perm |= PTE_W;

		KERN_DEBUG("ph:%x,zva:%x,eva:%x\n ", ph,zva,eva);

		for(; va < eva; va += PAGESIZE, fa += PAGESIZE) {
			KERN_DEBUG("va:%x,fa:%x,zva:%x\n",va,fa,zva);
			pmap_reserve(p->pmap, va, perm);
			if (va < ROUNDDOWN(zva, PAGESIZE)) // complete page
				pmap_copy(p->pmap, va,
					  pmap, (uint32_t)fa, PAGESIZE);
			else if (va < zva && ph->p_filesz) {    // partial
				pmap_memset(p->pmap, va, 0, PAGESIZE);
				pmap_copy(p->pmap, va, pmap, (uint32_t)fa, zva-va);
			} else                  // all-zero page
				pmap_memset(p->pmap, va, 0, PAGESIZE);
			pmap_setperm(p->pmap, va, PAGESIZE, perm);
		}
	}
}

uintptr_t
elf_entry(uintptr_t exe)
{
	elfhdr *eh = (elfhdr *) exe;
	KERN_ASSERT(eh->e_magic == ELF_MAGIC);
	return (uintptr_t) eh->e_entry;
}


void
load_elf(pmap_t *pmap_s, uintptr_t exe_s, pmap_t *pmap_d)
{
	KERN_ASSERT(pmap_s != NULL);
	KERN_ASSERT(pmap_d != NULL);

	//get the physical address of binary entry
	uintptr_t exe_k=pmap_la2pa(pmap_s,exe_s);
	elfhdr *eh_s = (elfhdr *) exe_s;
	elfhdr *eh_k = (elfhdr *) exe_k;
	KERN_ASSERT(eh_k->e_magic == ELF_MAGIC);

	// Load each program segment
	proghdr *ph = (proghdr *) pmap_la2pa(pmap_s,(uintptr_t) ( exe_s + eh_k->e_phoff));
	proghdr *eph = ph + eh_k->e_phnum;
	for (; ph < eph; ph++) {
		if (ph->p_type != ELF_PROG_LOAD)
			continue;

		void *fa = (void *) eh_s + ROUNDDOWN(ph->p_offset, PAGESIZE);
		uint32_t va = ROUNDDOWN(ph->p_va, PAGESIZE);
		uint32_t zva = ph->p_va + ph->p_filesz;
		uint32_t eva = ROUNDUP(ph->p_va + ph->p_memsz, PAGESIZE);

		uint32_t perm = PTE_P | PTE_U | PTE_G;
		if (ph->p_flags & ELF_PROG_FLAG_WRITE)
			perm |= PTE_W;

		for(; va < eva; va += PAGESIZE, fa += PAGESIZE) {
			pmap_reserve(pmap_d, va, perm);
			if (va < ROUNDDOWN(zva, PAGESIZE)) // complete page
				pmap_copy(pmap_d, va,
					  pmap_s, (uint32_t)fa, PAGESIZE);
			else if (va < zva && ph->p_filesz) {    // partial
				pmap_memset(pmap_d, va, 0, PAGESIZE);
				pmap_copy(pmap_d, va, pmap_s, (uint32_t)fa, zva-va);
			} else                  // all-zero page
				pmap_memset(pmap_d, va, 0, PAGESIZE);
			pmap_setperm(pmap_d, va, PAGESIZE, perm);
		}
	}
}
