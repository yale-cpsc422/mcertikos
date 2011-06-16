#ifndef _KERNEL_H
#define _KERNEL_H
#include <architecture/types.h>
#include <kern/proc/proc.h>

typedef struct cpu_use {
	procid_t running;
	bool stop;
	procid_t start;
} cpu_use;


#endif
