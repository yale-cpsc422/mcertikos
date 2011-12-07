#include <types.h>
#include <ext2.h>
#include <elf.h>
#include <x86.h>
#include <stdio.h>
#include <debug.h>
#include <bios.h>
#include <console.h>
#include <fs.h>
#include <ext2.h>

static void load_loader(uint32_t, uint32_t, bios_smap_t *);

void boot1main(uint32_t dev, uint32_t start_sect_idx, bios_smap_t *smap)
{
	cons_init();
	/* debug("Console is initialized.\n"); */

	/* debug("dev = %x, start_sect_idx = %x, smap = %x\n", */
	/*       dev, start_sect_idx, smap); */

	ext2_fs_init(dev, start_sect_idx);
	/* debug("Data structure for EXT2 filesystem is initialized.\n"); */

	cprintf("Load /boot/loader ...\n");
	load_loader(dev, start_sect_idx, smap);
}

static void load_loader(uint32_t dev, uint32_t start_sect_idx, bios_smap_t *smap)
{
	uint8_t elf_buf[SECTOR_SIZE * 8];
	elfhdr *ELFHDR = (elfhdr *)elf_buf;

	ext2_inode_t inode;
	uint32_t inode_idx;

	inode_idx = find_file("/boot/loader");
	if (inode_idx == EXT2_BAD_INO)
		panic("Cannot find the kernel.\n");
	read_inode(inode_idx, &inode);

	ext2_fsread(&inode, (uint8_t *)ELFHDR, 0, SECTOR_SIZE * 8);

	if (ELFHDR->e_magic != ELF_MAGIC)
		panic("/boot/loader is not a valid ELF file.\n");

	/* debug("Load sections\n"); */
	proghdr *ph = (proghdr *) ((uint8_t *) ELFHDR + ELFHDR->e_phoff);
	int i;
	for (i = 0; i < ELFHDR->e_phnum; i++, ph++) {
		ext2_fsread(&inode, (uint8_t *)ph->p_pa, ph->p_offset, ph->p_filesz);
	}

	/* debug("Start /boot/loader (%x)\n", ELFHDR->e_entry); */

	((void (*)(uint32_t, uint32_t, bios_smap_t *))
	 (ELFHDR->e_entry & 0xFFFFFFFF)) (dev, start_sect_idx, smap);
}
