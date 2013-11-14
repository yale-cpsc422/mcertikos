#ifndef _KERN_TRAP_SYSCALL_ARGS_H_
#define _KERN_TRAP_SYSCALL_ARGS_H_

#ifdef _KERN_

unsigned int syscall_get_arg1(void);
unsigned int syscall_get_arg2(void);
unsigned int syscall_get_arg3(void);
unsigned int syscall_get_arg4(void);
unsigned int syscall_get_arg5(void);
unsigned int syscall_get_arg6(void);

void syscall_set_errno(unsigned int errno);
void syscall_set_retval1(unsigned int retval);
void syscall_set_retval2(unsigned int retval);
void syscall_set_retval3(unsigned int retval);
void syscall_set_retval4(unsigned int retval);
void syscall_set_retval5(unsigned int retval);

/*
 * Primitives derived from lower layers.
 */

void pt_resv(unsigned int pid, unsigned int vaddr, unsigned int perm);
unsigned int pt_copyin(unsigned int pmap_id,
		       unsigned int uva, char *kva, unsigned int len);
unsigned int pt_copyout(char *kva,
			unsigned int pmap_id, unsigned int uva, unsigned int len);

unsigned int get_curid(void);

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

unsigned int uctx_get(unsigned int pid, unsigned int idx);

#endif /* _KERN_ */

#endif /* !_KERN_TRAP_SYSCALL_ARGS_H_ */
