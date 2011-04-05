/* See COPYRIGHT for copyright information. */

#ifndef PIOS_KERN_PTABLE_H
#define PIOS_KERN_PTABLE_H

#include <inc/arch/x86.h>
#include <kern/mem/mem.h>


// Page directory entries and page table entries are 32-bit integers.
typedef uint32_t pmap_t;

void pmap_init(void);
pmap_t* pmap_new(void);
void pmap_free(pmap_t* pmap);
pmap_t* pmap_insert(pmap_t* pmap, pageinfo* pi, uint32_t uva, int perm);
void pmap_remove(pmap_t* pmap, uint32_t uva, size_t size);
int pmap_setperm(pmap_t* pmap, uint32_t va, uint32_t size, int perm);

// for now this returns address plus permissions -- the whole PTE
uint32_t pmap_lookup(pmap_t* pmap, uint32_t);

void pmap_install(pmap_t* pmap);

#endif /* !PIOS_KERN_PTABLE_H */
