#ifndef _KERN_MM_MPMAP_H_
#define _KERN_MM_MPMAP_H_

#ifdef _KERN_

void pt_resv(int proc_index, unsigned int vaddr, int perm);
int  pt_copyin(int pmap_id, unsigned int uva, char *kva, unsigned int len);
int  pt_copyout(char *kva, int pmap_id, unsigned int uva, unsigned int len);
int  pt_memset(int pmap_id, unsigned int va, char c, unsigned int len);

/*
 * Derived from lower layers.
 */

void pmap_init(void);
int  pt_new(void);
void pt_free(int proc_idx);
int  pt_read(int proc_index, int va);
void pt_in(void);
void pt_out(void);
void pfree(int idx);
int  palloc(void);
void set_PT(int idx);

#endif /* !_KERN_ */

#endif /* !_KERN_MM_MPMAP_H_ */
