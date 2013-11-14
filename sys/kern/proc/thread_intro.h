#ifndef _KERN_PROC_THREAD_INTRO_H_
#define _KERN_PROC_THREAD_INTRO_H_

#ifdef _KERN_

/*
 * Primitives defined by this layer.
 */

void tcb_init(unsigned int pid);

unsigned int tcb_get_state(unsigned int pid);
void tcb_set_state(unsigned int pid, unsigned int state);

unsigned int tcb_get_prev(unsigned int pid);
void tcb_set_prev(unsigned int pid, unsigned int prev_pid);

unsigned int tcb_get_next(unsigned int pid);
void tcb_set_next(unsigned int pid, unsigned int next_pid);

/*
 * Primitives derived from lower layers.
 */

unsigned int palloc(void);
void pfree(unsigned int idx);

void pmap_init(unsigned int mbi_addr);

void pt_free(unsigned int pid);

unsigned int pt_read(unsigned int pid, unsigned int va);
void pt_resv(unsigned int pid, unsigned int vaddr, unsigned int perm);

void set_PT(unsigned int idx);

void pt_in(void);
void pt_out(void);

unsigned int pt_copyin(unsigned int pmap_id,
		       unsigned int uva, char *kva, unsigned int len);
unsigned int pt_copyout(char *kva,
			unsigned int pmap_id, unsigned int uva, unsigned int len);
unsigned int pt_memset(unsigned int pmap_id,
		       unsigned int va, char c, unsigned int len);

unsigned int kctx_new(void *entry);
void kctx_switch(unsigned int from_pid, unsigned int to_pid);

#endif /* _KERN_ */

#endif /* !_KERN_PROC_THREAD_INTRO_H_ */
