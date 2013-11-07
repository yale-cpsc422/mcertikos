#include <lib/debug.h>
#include <lib/string.h>
#include <lib/types.h>

#include <mm/export.h>

#define ELF_MAGIC 0x464C457FU	/* "\x7FELF" in little endian */

// ELF header
typedef struct elfhdf {
	uint32_t e_magic;	// must equal ELF_MAGIC
	uint8_t e_elf[12];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint32_t e_entry;
	uint32_t e_phoff;
	uint32_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
} elfhdr;

// ELF program header
typedef struct proghdr {
	uint32_t p_type;
	uint32_t p_offset;
	uint32_t p_va;
	uint32_t p_pa;
	uint32_t p_filesz;
	uint32_t p_memsz;
	uint32_t p_flags;
	uint32_t p_align;
} proghdr;

// ELF section header
typedef struct sechdr {
	uint32_t sh_name;
	uint32_t sh_type;
	uint32_t sh_flags;
	uint32_t sh_addr;
	uint32_t sh_offset;
	uint32_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint32_t sh_addralign;
	uint32_t sh_entsize;
} sechdr;

// Values for proghdr::p_type
#define ELF_PROG_LOAD		1

// Flag bits for proghdr::p_flags
#define ELF_PROG_FLAG_EXEC	1
#define ELF_PROG_FLAG_WRITE	2
#define ELF_PROG_FLAG_READ	4

// Values for sechdr::sh_type
#define ELF_SHT_NULL		0
#define ELF_SHT_PROGBITS	1
#define ELF_SHT_SYMTAB		2
#define ELF_SHT_STRTAB		3

// Values for sechdr::sh_name
#define ELF_SHN_UNDEF		0

#define PTE_P			0x001	/* Present */
#define PTE_W			0x002	/* Writeable */
#define PTE_U			0x004	/* User-accessible */

#define PAGESIZE		4096

void
elf_load(uintptr_t exe, int pmap_id)
{
	elfhdr *eh;
	proghdr *ph, *eph;
	sechdr *sh, *esh;
	char *strtab;
	uintptr_t bss_base, bss_size;

	eh = (elfhdr *) exe;

	KERN_ASSERT(eh->e_magic == ELF_MAGIC);
	KERN_ASSERT(eh->e_shstrndx != ELF_SHN_UNDEF);

	sh = (sechdr *) ((uintptr_t) eh + eh->e_shoff);
	esh = sh + eh->e_shnum;

	strtab = (char *) (exe + sh[eh->e_shstrndx].sh_offset);
	KERN_ASSERT(sh[eh->e_shstrndx].sh_type == ELF_SHT_STRTAB);

	bss_base = bss_size = 0;

	for (; sh < esh; sh++)
		if (strncmp(&strtab[sh->sh_name], ".bss", 4) == 0) {
			bss_base = sh->sh_addr;
			bss_size = sh->sh_size;
			break;
		}

	ph = (proghdr *) ((uintptr_t) eh + eh->e_phoff);
	eph = ph + eh->e_phnum;

	for (; ph < eph; ph++) {
		uintptr_t fa;
		uint32_t va, zva, eva, perm;

		if (ph->p_type != ELF_PROG_LOAD)
			continue;

		fa = (uintptr_t) eh + rounddown(ph->p_offset, PAGESIZE);
		va = rounddown(ph->p_va, PAGESIZE);
		zva = ph->p_va + ph->p_filesz;
		eva = roundup(ph->p_va + ph->p_memsz, PAGESIZE);

		perm = PTE_U | PTE_P;
		if (ph->p_flags & ELF_PROG_FLAG_WRITE)
			perm |= PTE_W;

		for(; va < eva; va += PAGESIZE, fa += PAGESIZE) {
			if (bss_base <= va &&
			    va + PAGESIZE <= bss_base + bss_size)
				continue;

			pt_resv(pmap_id, va, perm);

			if (va < rounddown(zva, PAGESIZE)) {
				/* copy a complete page */
				pt_copyout((void *) fa, pmap_id, va, PAGESIZE);
			} else if (va < zva && ph->p_filesz) {
				/* copy a partial page */
				pt_memset(pmap_id, va, 0, PAGESIZE);
				pt_copyout((void *) fa, pmap_id, va, zva-va);
			} else {
				/* zero a page */
				pt_memset(pmap_id, va, 0, PAGESIZE);
			}
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
