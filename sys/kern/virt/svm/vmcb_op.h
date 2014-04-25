#ifndef _KERN_VIRT_VMCB_OP_H_
#define _KERN_VIRT_VMCB_OP_H_

#ifdef _KERN_

void vmcb_set_intercept_vint(unsigned int enable);

void vmcb_clear_virq(void);
void vmcb_inject_virq(void);
void vmcb_inject_event(unsigned int type, unsigned int vector,
		       unsigned int errcode, unsigned int ev);

unsigned int vmcb_get_exit_info(unsigned int idx);
unsigned int vmcb_check_int_shadow(void);
unsigned int vmcb_check_pending_event(void);
unsigned int vmcb_get_next_eip(void);

void vmcb_set_seg(unsigned int seg, unsigned int sel,
		  unsigned int base, unsigned int lim, unsigned int ar);

void vmcb_set_reg(unsigned int reg, unsigned int val);
unsigned vmcb_get_reg(unsigned int reg);

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

void npt_insert(unsigned int gpa, unsigned int hpa);

void switch_to_guest(void);
void switch_to_host(void);

void vmcb_init(unsigned int mbi_addr);

unsigned int xvmst_read(unsigned int ofs);
void xvmst_write(unsigned int ofs, unsigned int v);

#endif /* _KERN_ */

#endif /* !_KERN_VIRT_VMCB_OP_H_ */
