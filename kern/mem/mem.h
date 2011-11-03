// Physical memory management definitions.
// See COPYRIGHT for copyright information.
#ifndef PIOS_KERN_MEM_H
#define PIOS_KERN_MEM_H

#include <architecture/types.h>
#include "pmem_layout.h"
#include "pages.h"

// At physical address MEM_IO (640K) there is a 384K hole for I/O.
// The hole ends at physical address MEM_EXT, where extended memory begins.
#define MEM_IO		0x0A0000
#define MEM_EXT		0x100000

//some consts for svm
#define START_PMEM_VMM 0x8000000
#define HOST_PMEM_SIZE 0xF000000
#define GUEST_PMEM_SIZE 0x4000000
#define GUEST_FIXED_PMEM_BYTES 0x4000000
#define PIOS_PMEM_START 0x100000

#define STACK_SIZE	(1 << 16) /* 64 KB */
#define	DEFAULT_VMM_PMEM_SIZE (1 << 24) /* 16 MB */
#define	DEFAULT_VMM_PMEM_START  0x100000// (1 << 20) /* 1 MB */

#define VMMSTACK_LO DEFAULT_VMM_PMEM_START+0x4000000 //assume the kernel image is less than 0x3F00000  
#define VMMSTACK_SIZE 0x4000000

// Given a physical address,
// return a C pointer the kernel can use to access it.
// This macro does nothing in PIOS because physical memory
// is mapped into the kernel's virtual address space at address 0,
// but this is not the case for many other systems such as JOS or Linux,
// which must do some translation here (usually just adding an offset).
#define mem_ptr(physaddr)	((void*)(physaddr))

// The converse to the above: given a C pointer, return a physical address.
#define mem_phys(ptr)		((uint32_t)(ptr))


// A pageinfo struct holds metadata on how a particular physical page is used.
// On boot we allocate a big array of pageinfo structs, one per physical page.
// This could be a union instead of a struct,
// since only one member is used for a given page state (free, allocated) -
// but that might make debugging a bit more challenging.
typedef struct pageinfo {
	struct pageinfo	*free_next;	// Next page number on free list
	int32_t	refcount;		// Reference count on allocated pages
} pageinfo;


// The pmem module sets up the following globals during mem_init().
extern size_t mem_max;		// Maximum physical address
extern size_t mem_npage;	// Total number of physical memory pages
extern pageinfo *mem_pageinfo;	// Metadata array indexed by page number
//extern static struct pmem_layout pml;

// Convert between pageinfo pointers, page indexes, and physical page addresses
#define mem_phys2pi(phys)	(&mem_pageinfo[(phys)/PAGESIZE])
#define mem_pi2phys(pi)		(((pi)-mem_pageinfo) * PAGESIZE)
#define mem_ptr2pi(ptr)		(mem_phys2pi(mem_phys(ptr)))
#define mem_pi2ptr(pi)		(mem_ptr(mem_pi2phys(pi)))


// Detect available physical memory and initialize the mem_pageinfo array.
void mem_init(const struct multiboot_info *mbi);

// Allocate a physical page and return a pointer to its pageinfo struct.
// Returns NULL if no more physical pages are available.
pageinfo *mem_alloc(void);

// Return a physical page to the free list.
void mem_free(pageinfo *pi);

void mem_incref(pageinfo *pp);
void mem_decref(pageinfo* pp);


struct pmem_layout * get_pmem_layout();

extern pageinfo * mem_alloc_contiguous( unsigned long n_pages);
extern unsigned long mem_alloc_one_page();
extern unsigned long alloc_host_pages ( unsigned long nr_pfns, unsigned long pfn_align );

#endif /* !PIOS_KERN_MEM_H */

