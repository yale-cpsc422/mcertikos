#ifndef _KERN_MEM_H_
#define _KERN_MEM_H_

#ifndef _KERN_
#error "This is a kernel header file; do not include it in userspace program."
#endif /* !_KERN_ */

#include <sys/types.h>

#include <machine/mem.h>

/* typedef struct pageinfo_t pageinfo_t; */

void mem_init(mboot_info_t *);

pageinfo_t *mem_page_alloc();
pageinfo_t *mem_pages_alloc(size_t);
void mem_page_free(pageinfo_t *);

void mem_incref(pageinfo_t *);
void mem_decref(pageinfo_t *);

uintptr_t mem_pi2phys(pageinfo_t *);
pageinfo_t *mem_phys2pi(uintptr_t);

size_t mem_max_phys(void);

#endif /* !_KERN_MEM_H_ */
