#ifndef __MULTIBOOT_H__
#define __MULTIBOOT_H__


//#include "types.h"
#include <architecture/types.h>

/* The magic number for the Multiboot header.  */
#define MULTIBOOT_HEADER_MAGIC		0x1BADB002

/* The flags for the Multiboot header.  */
//See http://www.gnu.org/software/grub/manual/multiboot/html_node/Header-magic-fields.html
// # define MULTIBOOT_HEADER_FLAGS		0x00010003
#define MULTIBOOT_HEADER_FLAGS		0x00000003

/* The magic number passed by a Multiboot-compliant boot loader.  */
#define MULTIBOOT_BOOTLOADER_MAGIC	0x2BADB002

#define MBI_CMDLINE    (1 << 2)
#define MBI_MODULES    (1 << 3)
#define MBI_MEMMAP     (1 << 6)

/* Do not include here in boot.S.  */
#ifndef __ASSEMBLY__

/* The symbol table for a.out.  */
struct aout_symbol_table
{
	uint32_t tabsize;
	uint32_t strsize;
	uint32_t addr;
	uint32_t reserved;
};

/* The section header table for ELF.  */
struct elf_section_header_table
{
	uint32_t num;
	uint32_t size;
	uint32_t addr;
	uint32_t shndx;
};

/* The Multiboot information.  */
// check http://www.gnu.org/software/grub/manual/multiboot/html_node/Boot-information-format.html
struct multiboot_info
{
	uint32_t flags;
	uint32_t mem_lower;
	uint32_t mem_upper;
	uint32_t boot_device;
	uint32_t cmdline;
	uint32_t mods_count;
	uint32_t mods_addr;
	union
	{
		struct aout_symbol_table aout_sym;
		struct elf_section_header_table elf_sec;
	} u;
	uint32_t mmap_length;
	uint32_t mmap_addr;
};

/* The module structure.  */
struct module_info
{
	uint32_t mod_start;
	uint32_t mod_end;
	uint32_t string;
	uint32_t reserved;
};

/* The memory map. Be careful that the offset 0 is base_addr_low
   but no size.  */
struct memory_map
{
	uint32_t size;
	uint32_t base_addr_low;
	uint32_t base_addr_high;
	uint32_t length_low;
	uint32_t length_high;
	uint32_t type;
};

#endif /* ! __ASSEMBLY__ */

#endif /* __MULTIBOOT_H__ */
