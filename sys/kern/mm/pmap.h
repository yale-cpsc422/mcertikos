#ifndef _KERN_MM_PMAP_H_
#define _KERN_MM_PMAP_H_

#ifdef _KERN_

#include <lib/export.h>

typedef uint32_t pmap_t;

/*
 * Initialize the page map module and turn on paging.
 */
void pmap_init(void);

/*
 * Create a page map for the process.
 * @return the page map structure if successful; otherwise, return NULL.
 */
pmap_t *pmap_new(void);

/*
 * Activate a page map.
 *
 * @param pmap the page map to be activated
 */
void pmap_install(pmap_t *pmap);

/*
 * Activate the kernel page map.
 */
void pmap_install_kern(void);

/*
 * Allocate a physical memory page and map a linear memory page to it in a page
 * map.
 *
 * @param pmap the page map to which the new mapping is inserted
 * @param la   the source linear memory page
 * @param perm the permission
 *
 * @return the page map structure if successful; otherwise, return NULL.
 */
int pmap_reserve(pmap_t *pmap, uintptr_t la, int perm);

/*
 * Convert a linear address to the physical address.
 *
 * @param pmap the page map used in the conversion
 * @param la   the linear address
 *
 * @return the physical address
 */
uintptr_t pmap_la2pa(pmap_t *pmap, uintptr_t la);

#endif /* _KERN_ */

#endif /* !_KERN_MM_PMAP_H_ */
