#ifndef _KERN_PROC_UCTX_H_
#define _KERN_PROC_UCTX_H_

#ifdef _KERN_

enum {
	U_EDI,
	U_ESI,
	U_EBP,
	U_OLD_ESP,
	U_EBX,
	U_EDX,
	U_ECX,
	U_EAX,
	U_ES,
	U_DS,
	U_TRAPNO,
	U_ERRNO,
	U_EIP,
	U_CS,
	U_EFLAGS,
	U_ESP,
	U_SS
};

void uctx_set(unsigned int pid, unsigned int idx, unsigned int val);
void uctx_set_eip(unsigned int pid, unsigned int eip);
unsigned int uctx_get(unsigned int pid, unsigned int idx);

/*
 * Primitives derived from lower layers.
 */

unsigned int palloc(void);
void pfree(unsigned int idx);

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

unsigned int get_curid(void);

void sched_init(unsigned int mbi_addr);

unsigned int thread_spawn(void *entry);
void thread_kill(unsigned int pid, unsigned chid);

void thread_wakeup(unsigned int chid);
void thread_sleep(void);
void thread_yield(void);

#endif /* _KERN_ */

#endif /* !_KERN_PROC_UCTX_H_ */
