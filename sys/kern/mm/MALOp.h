#ifndef _KERN_MM_MALOP_H_
#define _KERN_MM_MALOP_H_

#ifdef _KERN_

/*
 * Primitives defined by this layer
 */

void mem_init(unsigned int mbi_addr);
unsigned int palloc(void);
void pfree(unsigned int pfree_index);

/*
 * Derived from lower layers.
 */

void set_pe(void);
void set_pt(unsigned int *);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MALOP_H_ */
