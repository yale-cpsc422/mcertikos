#ifndef _MACHINE_PMAP_H_
#define _MACHINE_PMAP_H_

#ifndef _KERN_
#error "This is a kernel header; do not include it in userspace programs."
#endif

#include <sys/mem.h>
#include <sys/types.h>

// Page directory entries and page table entries are 32-bit integers.
typedef uintptr_t pmap_t;

void pmap_init(void);

pmap_t *pmap_new(void);
pmap_t *pmap_new_empty(void);
void pmap_free(pmap_t *);

pmap_t *pmap_insert(pmap_t *, pageinfo_t *, uintptr_t va, int perm);
pmap_t *pmap_reserve(pmap_t *, uintptr_t va, int perm);
void pmap_remove(pmap_t *, uintptr_t va, size_t size);

int pmap_setperm(pmap_t *, uintptr_t va, size_t size, int perm);

void pmap_install(pmap_t *pmap);

size_t pmap_copy(pmap_t *, uintptr_t, pmap_t *, uintptr_t, size_t);
size_t pmap_memset(pmap_t *, uintptr_t, char, size_t);

bool pmap_checkrange(pmap_t *, uintptr_t, size_t);

uintptr_t pmap_la2pa(pmap_t *, uintptr_t la);

pmap_t pmap_bootpdir[NPDENTRIES] gcc_aligned(PAGESIZE);
#endif /* !_MACHINE_PMAP_H_ */
