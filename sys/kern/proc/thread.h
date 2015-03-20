#ifndef _KERN_PROC_THREAD_H_
#define _KERN_PROC_THREAD_H_

#ifdef _KERN_

void sched_init(unsigned int mbi_addr);

unsigned int thread_spawn(void *entry);
void thread_kill(unsigned int pid, unsigned chid);

void thread_wakeup(unsigned int chid);
void thread_wakeup2(unsigned int tid);
void thread_sleep(unsigned int chid);
void thread_sleep2(void);
void thread_yield(void);

/*
 * Primitives derived from lower layers.
 */

unsigned int palloc(void);
void pfree(unsigned int idx);

unsigned int pt_read(unsigned int pid, unsigned int va);
void pt_resv(unsigned int pid, unsigned int vaddr, unsigned int perm);

void set_pt(unsigned int idx);

void pt_in(void);
void pt_out(void);

unsigned int pt_copyin(unsigned int pmap_id,
		       unsigned int uva, char *kva, unsigned int len);
unsigned int pt_copyout(char *kva,
			unsigned int pmap_id, unsigned int uva, unsigned int len);
unsigned int pt_memset(unsigned int pmap_id,
		       unsigned int va, char c, unsigned int len);

unsigned int get_curid(void);

#endif /* _KERN_ */

#endif /* !_KERN_PROC_THREAD_H_ */
