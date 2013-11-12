#ifndef _KERN_MM_MPTCOMM_H_
#define _KERN_MM_MPTCOMM_H_

#ifdef _KERN_

void pt_init_comm(unsigned int mbi_addr);

/*
 * Derived from lower layers.
 */

void pt_insert(unsigned int pid,
	       unsigned int va, unsigned int pa, unsigned int perm);
void pt_rmv(unsigned int pid, unsigned int va);
unsigned int pt_read(unsigned int pid, unsigned int va);
void pt_in(void);
void pt_out(void);
void pfree(unsigned int idx);
unsigned int palloc(void);
void set_pe(void);
void set_PT(unsigned int idx);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MPTCOMM_H_ */
