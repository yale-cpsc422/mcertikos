/********************************************************************************
* Derived from  XEN and MAVMM
* Adapted for CertiKOS by Liang Gu and Yale University
*
* This  module provides opreations for Hardware-based Virtual Machine
*
*
*********************************************************************************/

#include <architecture/types.h>
#include <inc/multiboot.h>
#include <kern/mem/e820.h>
#include <kern/debug/string.h>
#include <kern/debug/stdio.h>
#include <kern/mem/mem.h>


static void 
add_memory_region ( struct e820_map *e820, uint64_t start, uint64_t size, enum e820_type type )
{
	if ( e820->nr_map >= E820_MAX_ENTRIES ) {
		cprintf( "Too many entries in the memory map.\n" );
		return;
	}

	struct e820_entry *p = &e820->map [ e820->nr_map ];
	p->addr = start;
	p->size = size;
	p->type = type;

	e820->nr_map++;
}

void 
e820_print_map ( const struct e820_map *e820 )
{
	int i;

	cprintf( "BIOS-provided physical RAM map:\n" );

	for ( i = 0; i < e820->nr_map; i++ ) {
		const struct e820_entry *p = &e820->map [ i ];

		cprintf(" %x -", (unsigned long long) p->addr);
		cprintf(" %x ", (unsigned long long) ( p->addr + p->size ));
		switch ( p->type ) {
		case E820_RAM:			cprintf ( "(usable)\n" ); break;
		case E820_RESERVED: 	cprintf ( "(reserved)\n" ); break;
		case E820_ACPI:			cprintf ( "(ACPI data)\n" ); break;
		case E820_NVS:  		cprintf ( "(ACPI NVS)\n" ); break;
		default:				cprintf ( "type %x\n", p->type ); break;
		}
	}
}

void  setup_memory_region ( struct e820_map *e820, const struct multiboot_info *mbi )
{
	//first check that mbi->mmap_* fields are valid
	//check http://www.gnu.org/software/grub/manual/multiboot/html_node/Boot-information-format.html
	if (!(mbi->flags & MBI_MEMMAP)) cprintf( "Bootloader provided no memory information.\n" );

	e820->nr_map = 0;

	unsigned long p = 0;
	// go through memory_map entries in mbi, from mmap_addr to mmap_addr + mmap_length
	while ( p < mbi->mmap_length )
	{
		const struct memory_map *mmap = (struct memory_map *) (mbi->mmap_addr + p);

		const uint64_t start = ( (uint64_t)(mmap->base_addr_high) << 32) | (uint64_t) mmap->base_addr_low;
		const uint64_t size = ( (uint64_t)(mmap->length_high) << 32) | (uint64_t) mmap->length_low;

		//Debug
		//outf("start: %x - size: %x (%x, %x) - type: %x\n", start, size, mmap->length_low, mmap->length_high, mmap->type);

		//for each memory_map entry, add one corresponding entry to e820
		add_memory_region ( e820, start, size, mmap->type );

		//go to next memory_map entry
		p += mmap->size + sizeof (mmap->size);
	}

	// DEBUG
//	e820_print_map(e820);
}

// get the number of memory pages usable as RAM (has E820_RAM type)
unsigned long  get_nr_pages ( const struct e820_map *e820 )
{
	unsigned long n = 0;
	int i;

	for ( i = 0; i < e820->nr_map; i++ ) {
		const struct e820_entry *p = &e820->map [ i ];

		if (p->type != E820_RAM) continue;

		n += p->size >> PAGE_SHIFT;
	}

	return n;
}

// get the page number of the LAST memory page which is usable as RAM (has E820_RAM type)
unsigned long  get_max_pfn ( const struct e820_map *e820 )
{
	unsigned long n = 0;
	int i;

	for ( i = 0; i < e820->nr_map; i++ ) {
		const struct e820_entry *p = &e820->map [i];

		if ( p->type != E820_RAM ) continue;

		const unsigned long start = PFN_UP ( p->addr );
		const unsigned long end = PFN_DOWN ( p->addr + p->size );

		if (( start < end ) && ( end > n )) n = end;
	}

	return n;
}


// hide a memory region by marking it as reserved
void hide_memory ( struct multiboot_info * mbi, unsigned long hidestart, unsigned long length )
{
	unsigned long map_base = (unsigned long ) mbi->mmap_addr;
	unsigned long hideend = hidestart + length - 1;
	uint64_t replace_start, replace_size = 0;

	cprintf("Hiding the host - start: %x, length: %x\n", hidestart, length);

	uint32_t offset = 0;
	while (offset < mbi->mmap_length)
	{
		struct memory_map * mmap = (struct memory_map *) (map_base + offset);
		offset += mmap->size + sizeof(mmap->size);

		if ( mmap->type != E820_RAM )
		{
			// if has replace request and found an E820_RESERVED region to replace
			if ((replace_size != 0) && (mmap->type == E820_RESERVED))
			{
				cprintf("Replacing mmap entry, original start: %x, original size: %x, new start: %x, new size: %x\n",
						mmap->base_addr_low, mmap->length_low, replace_start, replace_size);
				mmap->type = E820_RAM;

				mmap->base_addr_low = replace_start;
				mmap->base_addr_high = replace_start >> 32;

				mmap->length_low = replace_size;
				mmap->length_high = replace_size >> 32;

				replace_size = 0; //clear replace request
			}

			continue;
		}

		uint64_t start = ( (uint64_t) mmap->base_addr_high << 32 ) | (uint64_t) mmap->base_addr_low;
		uint64_t size = ( (uint64_t) mmap->length_high << 32 ) | (uint64_t) mmap->length_low;
		uint64_t end = start + size;

		//Case 1: e820 regions cover hiding region => has to split
		if (start < hidestart && end > hideend)
		{
			cprintf("This original region was splited - start: %x, size: %x\n", start, size);

			// 1st split
			size = hidestart - start;
			mmap->length_low = size;
			mmap->length_high = size >> 32;

			cprintf("First split - start: %x, size: %x\n", start, size);

			// 2nd split
			start = hideend + 1;
			size = end - start;

			// by the map for this 2nd splitted region
			replace_size = size;
			replace_start = start;

			cprintf("Second split - start: %x, size: %x\n", start, size);
			continue;
		}

		//Case 2: hiding region covers or overlaps e820 region => don't have to split
		char b_interleave = 0;
		uint64_t newstart, newend;

		if ( (start >= hidestart) && (start <= hideend) )
		{
			newstart = hideend + 1; // Move newstart to after hiding region
			b_interleave = 1;
		}
		if ( (end >= hidestart) && (end <= hideend) )
		{
			newend = hidestart - 1; // Move newend to before hiding region
			b_interleave = 1;
		}

		if (b_interleave == 0) continue;

		if (end > start)	//if orginial e820 region interleves hiding region
		{
			cprintf("This region was adjusted - original start: %x, original size: %x, new start: %x, new size: %x\n",
					start, size, newstart, newend - newstart);

			mmap->base_addr_low = newstart;
			mmap->base_addr_high = newstart >> 32;

			size = newend - newstart;
			mmap->length_low = size;
			mmap->length_high = size >> 32;
		}
		else  // otherwise orginial e820 region is totally inside hiding region
		{
			mmap->type = E820_RESERVED;
			cprintf("This region was hidden - start: %x, size: %x\n", start, size);
		}
	}
}
