#ifndef _KERN_TRAP_TDISPATCH_H_
#define _KERN_TRAP_TDISPATCH_H_

#ifdef _KERN_

unsigned int syscall_get_arg1(void);
void syscall_set_errno(unsigned int errno);
void sys_puts(void);
void sys_spawn(void);
void sys_yield(void);
void sys_fork(void);

#endif  /* _KERN_ */

#endif  /* !_KERN_TRAP_TDISPATCH_H_ */
