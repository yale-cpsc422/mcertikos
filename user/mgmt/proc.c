#include <spinlock.h>

#include "proc.h"

struct proc {
	spinlock_t	lk;	/* spinlock should be acquired before access */

	pid_t		pid;	/* process id */
	uint32_t	cpu;	/* which CPU this process is running on? */
};
