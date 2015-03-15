#include "thread_intro.h"

#define NUM_PROC	64

void
thread_init(unsigned int mbi_addr)
{
	unsigned int pid;

	pmap_init(mbi_addr);

	pid = 0;
	while (pid < NUM_PROC) {
		tcb_init(pid);
		pid++;
	}
}
