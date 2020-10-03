#ifndef _KERN_TRAP_TSYSCALL_H_
#define _KERN_TRAP_TSYSCALL_H_

#ifdef _KERN_

void sys_puts(void);
void sys_spawn(void);
void sys_yield(void);

#endif  /* _KERN_ */

#endif  /* !_KERN_TRAP_TSYSCALL_H_ */
