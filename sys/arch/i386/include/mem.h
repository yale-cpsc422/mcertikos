#ifndef _MACHINE_MEM_H_
#define _MACHINE_MEM_H_

#ifdef _KERN_

#include <sys/mboot.h>
#include <sys/mmu.h>
#include <sys/types.h>

typedef
struct pmmap_t {
	uintptr_t	start;
	uintptr_t	end;
	uint32_t	type;
	struct pmmap_t	*next;
	struct pmmap_t	*type_next;
} pmmap_t;

pmmap_t pmmap[128];
pmmap_t *pmmap_usable, *pmmap_resv, *pmmap_acpi, *pmmap_nvs;

struct page_info;
typedef struct page_info pageinfo_t;

uintptr_t   mem_pi2phys(pageinfo_t *);
pageinfo_t *mem_phys2pi(uintptr_t);
void       *mem_pi2ptr(pageinfo_t *);
pageinfo_t *mem_ptr2pi(void *);

void   mem_init(mboot_info_t *);
size_t mem_max_phys(void);

pageinfo_t *mem_pages_alloc_align(size_t n, int p);

#define mem_page_alloc()			\
	mem_pages_alloc_align(1, 0)

#define mem_pages_alloc(size)						\
	mem_pages_alloc_align(ROUNDUP((size), PAGESIZE)/PAGESIZE, 0)

pageinfo_t *mem_pages_free(pageinfo_t *);

#define mem_page_free(page)			\
	mem_pages_free((page))

void mem_incref(pageinfo_t *);
void mem_decref(pageinfo_t *);

#endif /* _KERN_ */

#endif /* !_MACHINE_MEM_H_ */
