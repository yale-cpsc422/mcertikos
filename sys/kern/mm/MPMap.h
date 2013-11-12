#ifndef _KERN_MM_MPMAP_H_
#define _KERN_MM_MPMAP_H_

#ifdef _KERN_

void pt_resv(unsigned int pid, unsigned int vaddr, unsigned int perm);

unsigned int pt_copyin(unsigned int pmap_id,
		       unsigned int uva, char *kva, unsigned int len);
unsigned int pt_copyout(char *kva,
			unsigned int pmap_id, unsigned int uva, unsigned int len);
unsigned int pt_memset(unsigned int pmap_id,
		       unsigned int va, char c, unsigned int len);

/*
 * Derived from lower layers.
 */

void pmap_init(unsigned int mbi_addr);
unsigned int pt_new(void);
void pt_free(unsigned int pid);
unsigned int pt_read(unsigned int pid, unsigned int va);
void pt_in(void);
void pt_out(void);
void pfree(unsigned int idx);
unsigned int palloc(void);
void set_PT(unsigned int idx);

#endif /* !_KERN_ */

#endif /* !_KERN_MM_MPMAP_H_ */
