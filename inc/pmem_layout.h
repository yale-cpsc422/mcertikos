#ifndef __PMEM_LAYOUT_H__
#define __PMEM_LAYOUT_H__

#include <kern/mem/e820.h>

struct pmem_layout {
	unsigned long max_page;		// the page of the max address in the available region of phsyical memory
	unsigned long total_pages;	// the total number of all memories
	unsigned long max_address;
	
	struct Elf_Ehdr *ehdr_pios ;

	unsigned long vmm_pmem_start;
	unsigned long vmm_heap_start;
	unsigned long host_ab_start;
	unsigned long vmm_pmem_end;//4kpage

	struct e820_map e820;
};

#endif /* __PMEM_LAYOUT_H__ */

