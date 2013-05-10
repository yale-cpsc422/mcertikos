#ifndef _MACHINE_PMAP_H_
#define _MACHINE_PMAP_H_

#ifdef _KERN_

#include <sys/mem.h>
#include <sys/types.h>

typedef uintptr_t pmap_t;

/*
 * Get the kernel page map.
 */
pmap_t *pmap_kern_map(void);

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
 * Free the memory allocated for a page map of a process.
 */
void pmap_free(pmap_t *);

/*
 * Map a linear memory page to a physical memory page in a page map.
 *
 * @param pmap the page map to which the new mapping is inserted
 * @param pi   the target physical memory page
 * @param la   the source linear memory page
 * @param perm the permission
 *
 * @return the page map structure if successful; otherwise, return NULL.
 */
pmap_t *pmap_insert(pmap_t *pmap, pageinfo_t *pi, uintptr_t la, int perm);

/*
 * Allocate a physical memory page and map a linear memory page to it in a page
 * map.
 *
 * XXX: pmap_reserve() prefers to allocating high physical memory unless there
 *      is not enough high physical memory.
 *
 * @param pmap the page map to which the new mapping is inserted
 * @param la   the source linear memory page
 * @param perm the permission
 *
 * @return the page map structure if successful; otherwise, return NULL.
 */
pmap_t *pmap_reserve(pmap_t *pmap, uintptr_t la, int perm);

/*
 * Unmap a linear address region in a page map.
 *
 * @param pmap the page map from which the mappings are unmapped
 * @param la   the start linear address of the region to be unmapped
 * @param size how many byte will be unmapped
 */
void pmap_remove(pmap_t *pmap, uintptr_t la, size_t size);

/*
 * Set the permission of a linear address region.
 *
 * @param pmap the page map where the linear address region is
 * @param la   the start linear address of the linear address region
 * @param size how many bytes are in the linear address region
 * @param perm the permission
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int pmap_setperm(pmap_t *pmap, uintptr_t la, size_t size, int perm);

/*
 * Activate a page map.
 *
 * @param pmap the page map to be activated
 */
void pmap_install(pmap_t *pmap);

/*
 * Transfer data between two page maps.
 *
 * @param d_pmap the destination page map
 * @param d_la   the destination linear address
 * @param s_pmap the source page map
 * @param s_la   the source linear address
 * @param size   how many byes will be transfered
 *
 * @return the actual number of bytes copied
 */
size_t pmap_copy(pmap_t *d_pmap, uintptr_t d_la,
		 pmap_t *s_pmap, uintptr_t s_la, size_t size);

/*
 * Set the bytes in a linear address region in a page map to a specified value.
 *
 * @param pmap the page map where the linear address region is
 * @param la   the start linear address of the region
 * @param c    the speficied value
 * @param size how many bytes in the region will set
 *
 * @return the actual number of bytes set
 */
size_t pmap_memset(pmap_t *pmap, uintptr_t la, char c, size_t size);

bool pmap_checkrange(pmap_t *, uintptr_t, size_t);

uintptr_t pmap_la2pa(pmap_t *, uintptr_t la);

#endif /* _KERN_ */

#endif /* !_MACHINE_PMAP_H_ */
