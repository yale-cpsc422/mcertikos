#ifndef _KERN_MM_MPTBIT_H_
#define _KERN_MM_MPTBIT_H_

#ifdef _KERN_

void set_bit(unsigned int pid, unsigned int val);
unsigned int is_used(unsigned int pid);

/*
 * Derived from lower layers.
 */

void pt_init(unsigned int mbi_addr);
unsigned int pt_insert(unsigned int pid,
	       unsigned int va, unsigned int pa, unsigned int perm);
unsigned int pt_rmv(unsigned int pid, unsigned int va);
void pt_in(void);
void pt_out(void);
void pfree(unsigned int idx);
unsigned int palloc(void);
void set_pt(unsigned int idx);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MPTBIT_H_ */
