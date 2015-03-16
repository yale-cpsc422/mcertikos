#include <lib/gcc.h>

#include "kctx.h"

#define NUM_PROC	64
#define PAGESIZE	4096

char STACK_LOC[NUM_PROC][PAGESIZE] gcc_aligned(PAGESIZE);

unsigned int
kctx_new(void *entry)
{
	unsigned int pid;

	pid = pt_new();

	kctx_set_esp(pid, (void *) (&STACK_LOC[pid][PAGESIZE-4]));
	kctx_set_eip(pid, entry);

	return pid;
}
