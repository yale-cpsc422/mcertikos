#ifndef _USER_PROC_H_
#define _USER_PROC_H_

#include <gcc.h>
#include <types.h>

pid_t	spawn(uintptr_t);
void	yield(void);
pid_t	getpid(void);

struct msg {
	pid_t	pid;				/* process id of the sender */
	char	data[256-sizeof(pid_t)];	/* data payload */
} gcc_packed;

size_t	recv(struct msg *);
void	send(pid_t, void *, size_t);

#endif /* !_USER_PROC_H_ */
