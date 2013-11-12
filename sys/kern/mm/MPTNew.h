#ifndef _KERN_MM_MPTNEW_H_
#define _KERN_MM_MPTNEW_H_

#ifdef _KERN_

void pmap_init(unsigned int mbi_addr);
unsigned int pt_new(void);
void pt_free(unsigned int pid);

/*
 * Derived from lower layers.
 */

void pt_insert(unsigned int pid,
	       unsigned int va, unsigned int pa, unsigned int perm);
unsigned int pt_read(unsigned int pid, unsigned int va);
void pt_in(void);
void pt_out(void);
void pfree(unsigned int idx);
unsigned int palloc(void);
void set_PT(unsigned int idx);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MPTNEW_H_ */
