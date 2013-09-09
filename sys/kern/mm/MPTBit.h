#ifndef _KERN_MM_MPTBIT_H_
#define _KERN_MM_MPTBIT_H_

#ifdef _KERN_

void set_bit(int proc_idx, int val);
int  is_used(int proc_idx);

/*
 * Derived from lower layers.
 */

void pt_init(unsigned int mbi_addr);
void pt_insert(int proc_index, unsigned int va, unsigned int pa, int perm);
void pt_rmv(int proc_index, unsigned int va);
unsigned int pt_read(int proc_index, unsigned int va);
void pt_in(void);
void pt_out(void);
void pfree(int idx);
int  palloc(void);
void set_PT(int idx);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MPTBIT_H_ */
