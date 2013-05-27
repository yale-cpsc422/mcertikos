#include <sys/debug.h>
#include <sys/elf.h>
#include <sys/mmu.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <machine/pmap.h>

/*
 * Load elf execution file exe to the virtual address space pmap.
 */
void
elf_load(uintptr_t exe, pmap_t *pmap)
{
	KERN_ASSERT(pmap != NULL);

	pmap_t *cur_pmap;
	elfhdr *eh;
	proghdr *ph, *eph;

	cur_pmap = pmap_kern_map();

	if (cur_pmap == pmap)
		return;

	eh = (elfhdr *) exe;

	KERN_ASSERT(eh->e_magic == ELF_MAGIC);

	ph = (proghdr *) ((uintptr_t) eh + eh->e_phoff);
	eph = ph + eh->e_phnum;

	for (; ph < eph; ph++) {
		uintptr_t fa;
		uint32_t va, zva, eva, perm;

		if (ph->p_type != ELF_PROG_LOAD)
			continue;

		fa = (uintptr_t) eh + ROUNDDOWN(ph->p_offset, PAGESIZE);
		va = ROUNDDOWN(ph->p_va, PAGESIZE);
		zva = ph->p_va + ph->p_filesz;
		eva = ROUNDUP(ph->p_va + ph->p_memsz, PAGESIZE);

		perm = PTE_P | PTE_U;
		if (ph->p_flags & ELF_PROG_FLAG_WRITE)
			perm |= PTE_W;

		for(; va < eva; va += PAGESIZE, fa += PAGESIZE) {
			if (pmap_reserve(pmap, va, perm) == NULL)
				KERN_PANIC("Cannot allocate memory for "
					   "ELF file (va 0x%08x, perm %x).\n",
					   va, perm);

			if (va < ROUNDDOWN(zva, PAGESIZE)) {
				/* copy a complete page */
				pmap_copy(pmap, va,
					  cur_pmap, (uint32_t)fa, PAGESIZE);
			} else if (va < zva && ph->p_filesz) {
				/* copy a partial page */
				pmap_memset(pmap, va, 0, PAGESIZE);
				pmap_copy(pmap, va,
					  cur_pmap, (uint32_t)fa, zva-va);
			} else {
				/* zero a page */
				pmap_memset(pmap, va, 0, PAGESIZE);
			}

			/* set permission */
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
