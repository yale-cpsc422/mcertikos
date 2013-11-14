#ifndef _VIRT_SVM_H_
#define _VIRT_SVM_H_

#ifdef _KERN_

void svm_set_intercept_intwin(unsigned int enable);

void svm_set_reg(unsigned int reg, unsigned int val);

unsigned int svm_get_reg(unsigned int reg);
unsigned int svm_get_exit_reason(void);
unsigned int svm_get_exit_io_port(void);
unsigned int svm_get_exit_io_width(void);
unsigned int svm_get_exit_io_write(void);
unsigned int svm_get_exit_io_rep(void);
unsigned int svm_get_exit_io_str(void);
unsigned int svm_get_exit_io_neip(void);
unsigned int svm_get_exit_fault_addr(void);

void svm_sync(void);
void svm_run_vm(void);

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

unsigned int get_curid(void);

void thread_kill(unsigned int pid, unsigned int chid);

void thread_wakeup(unsigned int chid);
void thread_sleep(void);
void thread_yield(void);

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
unsigned int uctx_get(unsigned int pid, unsigned int idx);

unsigned int proc_create(void *elf_addr);
void proc_start_user(void);

void npt_insert(unsigned int gpa, unsigned int hpa);

void vmcb_init(unsigned int mbi_addr);

void vmcb_set_intercept_vint(unsigned int enable);
void vmcb_inject_event(unsigned int type, unsigned int vector,
		       unsigned int errcode, unsigned int ev);
unsigned int vmcb_check_int_shadow(void);
unsigned int vmcb_check_pending_event(void);
unsigned int vmcb_get_next_eip(void);
void vmcb_set_seg(unsigned int seg, unsigned int sel,
		  unsigned int base, unsigned int lim, unsigned int ar);

#endif /* _KERN_ */

#endif /* !_VIRT_SVM_H_ */
