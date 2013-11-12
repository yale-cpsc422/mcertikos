#ifndef _KERN_MM_MPTOP_H_
#define _KERM_MM_MPTOP_H_

#ifdef _KERN_

/*
 * Primitives defined by this layer.
 */

void pt_insert(unsigned int pid, unsigned int va, unsigned int pa, unsigned int perm);
void pt_rmv(unsigned int pid, unsigned int va);
unsigned int pt_read(unsigned int pid, unsigned int va);

/*
 * Primitives derived from lower layers.
 */

void set_PDX(unsigned int pid, unsigned int pdx);
void pt_in(void);
void pt_out(void);
void pfree(unsigned int idx);
unsigned int palloc(void);
void mem_init(unsigned int mbi_addr);
void set_pe(void);
void set_PT(unsigned int idx);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MPTOP_H_ */
