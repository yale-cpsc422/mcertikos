#ifndef _MACHINE_MEM_H_
#define _MACHINE_MEM_H_

#ifndef _KERN_
#error "This is a kernel header; do not include it in userspace programs."
#endif

#include <sys/mboot.h>
#include <sys/mmu.h>
#include <sys/types.h>

typedef
struct pageinfo_t {
	struct pageinfo_t *free_next;
	int32_t refcount;
} pageinfo_t;

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

void mem_init(mboot_info_t *);

size_t mem_max_phys(void);

pageinfo_t *mem_page_alloc();
void mem_page_free(pageinfo_t *);

uintptr_t mem_pi2phys(pageinfo_t *);
pageinfo_t *mem_phys2pi(uintptr_t);

void *mem_pi2ptr(pageinfo_t *);
pageinfo_t *mem_ptr2pi(void *);

#endif /* !_MACHINE_MEM_H_ */
