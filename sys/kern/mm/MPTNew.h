#ifndef _KERN_MM_MPTNEW_H_
#define _KERN_MM_MPTNEW_H_

#ifdef _KERN_

void pmap_init(unsigned int mbi_addr);
int  pt_new(void);
void pt_free(int proc_idx);

/*
 * Derived from lower layers.
 */

void pt_insert(int proc_index, unsigned int va, unsigned int pa, int perm);
unsigned int pt_read(int proc_index, unsigned int va);
void pt_in(void);
void pt_out(void);
void pfree(int idx);
int  palloc(void);
void set_PT(int idx);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MPTNEW_H_ */
