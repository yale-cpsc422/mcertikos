/* See COPYRIGHT for copyright information. */

#ifndef PIOS_KERN_AS_H
#define PIOS_KERN_AS_H

#include <architecture/x86.h>
#include <kern/mem/mem.h>

// Page directory entries and page table entries are 32-bit integers.
typedef uint32_t as_t;

as_t* as_init(void);
as_t* as_current();
void as_activate(as_t* as);
as_t* as_new(void);
void as_free(as_t* as);
// as_t* as_clone(as_t* as);
as_t* as_reserve(as_t* as, uint32_t uva, int perm);
as_t* as_assign(as_t* as, uint32_t uva, int perm, pageinfo* pi);
as_t* as_remove(as_t* as, uint32_t uva, size_t size);
as_t* as_disconnect(as_t* as, uint32_t uva, size_t size);
int as_getperm(as_t* as, uint32_t uva);
as_t* as_setperm(as_t* as, uint32_t va, uint32_t size, int perm);

// Should this be here?
//void as_inval(as* as, uint32_t uva, size_t size);

bool as_checkrange(as_t *as, uint32_t va, size_t size);
bool as_copy(as_t* das, uint32_t dva, as_t* sas, uint32_t sva, size_t size);
void as_memset(as_t* as, uint32_t va, char v, size_t size);

#endif /* !PIOS_KERN_AS_H */
