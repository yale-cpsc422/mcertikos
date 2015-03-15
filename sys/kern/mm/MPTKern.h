#ifndef _KERN_MM_MPTKERN_H_
#define _KERN_MM_MPTKERN_H_

#ifdef _KERN_

void pt_init_kern(unsigned int mbi_addr);
unsigned int pt_insert(unsigned int, unsigned int, unsigned int, unsigned int);
unsigned int pt_rmv(unsigned int, unsigned int);

/*
 * Derived from lower layers.
 */

void pt_in(void);
void pt_out(void);
void pfree(unsigned int idx);
unsigned int palloc(void);
void set_pg(void);
void set_pt(unsigned int idx);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MPTKERN_H_ */
