#ifndef _KERN_TRAP_INTERRUPT_H_
#define _KERN_TRAP_INTERRUPT_H_

#ifdef _KERN_

void set_PT(unsigned int pid);

unsigned int get_curid(void);

void proc_start_user(void);

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

unsigned int uctx_get(unsigned int pid, unsigned int idx);

void interrupt_handler(void);
void syscall_dispatch(void);
void exception_handler(void);

#endif /* _KERN_ */

#endif /* !_KERN_TRAP_INTERRUPT_H_ */
