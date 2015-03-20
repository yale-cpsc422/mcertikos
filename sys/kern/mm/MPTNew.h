#ifndef _KERN_MM_MPTNEW_H_
#define _KERN_MM_MPTNEW_H_

#ifdef _KERN_

unsigned int pt_resv(unsigned int, unsigned int, unsigned int);
void pt_resv2(unsigned int, unsigned int, unsigned int, unsigned int);
void pmap_init(unsigned int mbi_addr);
unsigned int pt_new(void);

/*
 * Derived from lower layers.
 */

unsigned int pt_insert(unsigned int pid,
	       unsigned int va, unsigned int pa, unsigned int perm);
unsigned int pt_read(unsigned int, unsigned int);
void pt_in(void);
void pt_out(void);
void pfree(unsigned int idx);
unsigned int palloc(void);
void set_pt(unsigned int idx);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MPTNEW_H_ */
