#ifndef _KERN_VIRT_NPT_INTRO_H_
#define _KERN_VIRT_NPT_INTRO_H_

#ifdef _KERN_

#include <lib/gcc.h>

#define PAGESIZE	4096

struct NPTStruct {
	char		*pdir[1024]	gcc_aligned(PAGESIZE);
	unsigned int	pt[1024][1024]	gcc_aligned(PAGESIZE);
};

/*
 * Primitives defined by this layer.
 */

void set_NPDE(unsigned int pdx);
void set_NPTE(unsigned int pdx, unsigned int ptx, unsigned int paddr);

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

void sched_init(unsigned int mbi_addr);

void thread_kill(unsigned int pid, unsigned chid);

void thread_wakeup(unsigned int chid);
void thread_sleep(unsigned int chid);
void thread_yield(void);

void uctx_set(unsigned int pid, unsigned int idx, unsigned int val);
unsigned int uctx_get(unsigned int pid, unsigned int idx);

#endif /* _KERN_ */

#endif /* !_KERN_VIRT_NPT_INTRO_H_ */
