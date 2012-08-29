#ifndef _USER_PROC_H_
#define _USER_PROC_H_

#include <gcc.h>
#include <types.h>

pid_t	spawn(uint32_t cpu_idx, uintptr_t exe_addr);
void	yield(void);
pid_t	getpid(void);

#endif /* !_USER_PROC_H_ */
