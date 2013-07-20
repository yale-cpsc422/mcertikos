#ifndef _KERN_MM_EXPORT_H_
#define _KERN_MM_EXPORT_H_

#ifdef _KERN_

#define PAGESIZE	4096

#define VM_USERHI	0xf0000000
#define VM_USERLO	0x40000000

#define PTE_P		0x001	/* Present */
#define PTE_W		0x002	/* Writeable */
#define PTE_U		0x004	/* User-accessible */

#define PFE_PR		0x1	/* Page fault caused by protection violation */

struct mboot_info;
typedef uint32_t	pmap_t;

void mem_init(struct mboot_info *mbi);
void pmap_init(void);
pmap_t *pmap_new(void);
pmap_t *pmap_kern_map(void);
void pmap_install_kern(void);
void pmap_install(pmap_t *pmap);
int pmap_reserve(pmap_t *pmap, uintptr_t va, int perm);
int pmap_copy(pmap_t *dst_pmap, uintptr_t dst_la,
	      pmap_t *src_pmap, uintptr_t src_la, size_t size);
uintptr_t pmap_la2pa(pmap_t *pmap, uintptr_t va);

#endif /* _KERN_ */

#endif /* !_KERN_MM_EXPORT_H_ */
