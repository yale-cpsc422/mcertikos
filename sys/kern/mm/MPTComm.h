#ifndef _KERN_MM_MPTCOMM_H_
#define _KERN_MM_MPTCOMM_H_

#ifdef _KERN_

void pt_init_comm(unsigned int mbi_addr);

/*
 * Derived from lower layers.
 */

void pt_insert(int proc_index, unsigned int va, unsigned int pa, int perm);
void pt_rmv(int proc_index, unsigned int va);
unsigned int pt_read(int proc_index, unsigned int va);
void pt_in(void);
void pt_out(void);
void pfree(int idx);
int  palloc(void);
void set_pe(void);
void set_PT(int idx);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MPTCOMM_H_ */
