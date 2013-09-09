#ifndef _KERN_MM_MAL_H_
#define _KERN_MM_MAL_H_

#ifdef _KERN_

void pfree(int idx);
int  palloc(void);

/*
 * Derived from lower layers.
 */

void mem_init(unsigned int mbi_addr);
void set_pe(void);
void set_pt(void *);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MAL_H_ */
