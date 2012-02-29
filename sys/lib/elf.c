#include <sys/as.h>
#include <sys/debug.h>
#include <sys/elf.h>
/* #include <sys/fs.h> */
#include <sys/mmu.h>
#include <sys/string.h>
#include <sys/types.h>

/* static uint8_t elf_fs_buf[PAGESIZE]; */

void
elf_load(as_t *as, uintptr_t exe)
{
	KERN_ASSERT(as != NULL);

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

		uint32_t perm = PTE_P | PTE_U | PTE_W;
		if (ph->p_flags & ELF_PROG_FLAG_WRITE)
			perm |= PTE_W;

		for(; va < eva; va += PAGESIZE, fa += PAGESIZE) {
			//pageinfo *pi = mem_alloc(); assert(pi != NULL);
			//cprintf("reserving %x\n", va);
			as_reserve(as, va, perm);
			KERN_ASSERT(as);
			if (va < ROUNDDOWN(zva, PAGESIZE)) // complete page
				as_copy(as,va, as, (uint32_t)fa, PAGESIZE);
			else if (va < zva && ph->p_filesz) {    // partial
				as_memset(as, va, 0, PAGESIZE);
				as_copy(as,va, as, (uint32_t)fa, zva-va);
			} else                  // all-zero page
				as_memset(as, va, 0, PAGESIZE);
			as_setperm(as, va, perm, PAGESIZE);
		}
	}
}

/* int */
/* elf_load_file(as_t *as, const char *file) */
/* { */
/* 	KERN_ASSERT(as != NULL); */

/* 	uint8_t exe[SECTOR_SIZE * 8]; */
/* 	elfhdr *eh = (elfhdr *) exe; */

/* 	ext2_inode_t inode; */
/* 	uint32_t inode_idx; */

/* 	/\* load ELF header *\/ */
/* 	inode_idx = find_file(kernel_path); */

/* 	if (inode_idx == EXT2_BAD_INO) { */
/* 		KERN_DEBUG("Cannot find %s.\n", file); */
/* 		return 1; */
/* 	} */

/* 	read_inode(inode_idx, &inode); */
/* 	ext2_fsread(&inode, (uint8_t *)ELFHDR, 0, SECTOR_SIZE * 8); */

/* 	if (eh->e_magic == ELF_MAGIC) { */
/* 		KERN_DEBUG("%s is not an ELF file.\n", file); */
/* 		return 1; */
/* 	} */

/* 	/\* load each program segment *\/ */
/* 	proghdr *ph = (proghdr *) ((void *) eh + eh->e_phoff); */
/* 	proghdr *eph = ph + eh->e_phnum; */
/* 	for (; ph < eph; ph++) { */
/* 		if (ph->p_type != ELF_PROG_LOAD) */
/* 			continue; */

/* 		uint32_t fa = ROUNDDOWN(ph->p_offset, PAGESIZE); */
/* 		uint32_t sz = ph->p_filesz; */
/* 		uint32_t va = ROUNDDOWN(ph->p_va, PAGESIZE); */
/* 		uint32_t zva = ph->p_va + ph->p_filesz; */
/* 		uint32_t eva = ROUNDUP(ph->p_va + ph->p_memsz, PAGESIZE); */

/* 		uint32_t perm = PTE_P | PTE_U; */
/* 		if (ph->p_flags & ELF_PROG_FLAG_WRITE) */
/* 			perm |= PTE_W; */

/* 		for(; va < eva; va += PAGESIZE, fa += PAGESIZE) { */
/* 			as_reserve(as, va, perm); */

/* 			if (va < ROUNDDOWN(zva, PAGESIZE)) { /\* complete page *\/ */
/* 				ext2_fsread(&inode, elf_buf, fa, PAGESIZE); */
/* 				as_copy(as, va, kern_as, elf_buf, PAGESIZE); */
/* 			} else if (va < zva && ph->p_filesz) { /\* partial *\/ */
/* 				ext2_fsread(&inode, elf_buf, fa, zva - va); */
/* 				as_memset(as, va, 0, PAGESIZE); */
/* 				as_copy(as, va, kern_as, elf_buf, zva - va); */
/* 			} else                  // all-zero page */
/* 				as_memset(as, va, 0, PAGESIZE); */
/* 			as_setperm(as, va, perm, PAGESIZE); */
/* 		} */
/* 	} */

/* 	return 0; */
/* } */

uintptr_t
elf_entry(uintptr_t exe)
{
	elfhdr *eh = (elfhdr *) exe;
	KERN_ASSERT(eh->e_magic == ELF_MAGIC);
	return (uintptr_t) eh->e_entry;
}
