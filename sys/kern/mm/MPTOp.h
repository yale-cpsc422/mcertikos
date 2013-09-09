#ifndef _KERN_MM_MPTOP_H_
#define _KERM_MM_MPTOP_H_

#ifdef _KERN_

void pt_insert(int proc_index, unsigned int va, unsigned int pa, int perm);
void pt_rmv(int proc_index, unsigned int va);
unsigned int pt_read(int proc_index, unsigned int va);
void pt_unpresent(int proc_index, unsigned int va);

/*
 * Derived from lower layers.
 */

void set_PDX(int proc_idx, int pdx);
void pt_in(void);
void pt_out(void);
void pfree(int idx);
int  palloc(void);
void mem_init(unsigned int mbi_addr);
void set_pe(void);
void set_PT(int idx);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MPTOP_H_ */
