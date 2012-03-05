#include <sys/debug.h>
#include <sys/elf.h>
#include <sys/mmu.h>
#include <sys/string.h>
#include <sys/types.h>

#include <machine/pmap.h>

void
elf_load(pmap_t *pmap, uintptr_t exe)
{
	KERN_ASSERT(pmap != NULL);

	elfhdr *eh = (elfhdr *) exe;
	KERN_ASSERT(eh->e_magic == ELF_MAGIC);

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

		for(; va < eva; va += PAGESIZE, fa += PAGESIZE) {
			pmap_reserve(pmap, va, perm);
			if (va < ROUNDDOWN(zva, PAGESIZE)) // complete page
				pmap_copy(pmap, va,
					  pmap, (uint32_t)fa, PAGESIZE);
			else if (va < zva && ph->p_filesz) {    // partial
				pmap_memset(pmap, va, 0, PAGESIZE);
				pmap_copy(pmap, va, pmap, (uint32_t)fa, zva-va);
			} else                  // all-zero page
				pmap_memset(pmap, va, 0, PAGESIZE);
			pmap_setperm(pmap, va, PAGESIZE, perm);
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
