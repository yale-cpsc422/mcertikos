#ifndef _KERN_VIRT_SVM_SWITCH_H_
#define _KERN_VIRT_SVM_SWITCH_H_

#ifdef _KERN_

/*
 * Primitives defined by this layer.
 */

void save_hctx(void);
void restore_hctx(void);

/*
 * Primitives derived from lower layers.
 */

unsigned int palloc(void);
void pfree(unsigned int idx);

unsigned int pt_read(unsigned int pid, unsigned int va);
void pt_resv(unsigned int pid, unsigned int vaddr, unsigned int perm);

unsigned int pt_copyin(unsigned int pmap_id,
		       unsigned int uva, char *kva, unsigned int len);
unsigned int pt_copyout(char *kva,
			unsigned int pmap_id, unsigned int uva, unsigned int len);
unsigned int pt_memset(unsigned int pmap_id,
		       unsigned int va, char c, unsigned int len);

unsigned int get_curid(void);

void thread_kill(unsigned int pid, unsigned chid);

void thread_wakeup(unsigned int chid);
void thread_sleep(unsigned int chid);
void thread_yield(void);

void uctx_set(unsigned int pid, unsigned int idx, unsigned int val);
unsigned int uctx_get(unsigned int pid, unsigned int idx);

void npt_init(unsigned int mbi_addr);
void npt_insert(unsigned int gpa, unsigned int hpa);

#endif /* _KERN_ */

#endif /* !_KERN_VIRT_SVM_SWITCH_H_ */
