#ifndef _USER_PROC_H_
#define _USER_PROC_H_

#include <types.h>

pid_t spawn(uint32_t cpu_idx, uintptr_t exe, chid_t chid);
void  yield(void);
pid_t getpid(void);
pid_t getppid(void);

#endif /* !_USER_PROC_H_ */
