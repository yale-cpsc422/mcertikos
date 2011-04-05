// Physical memory management definitions.
// See COPYRIGHT for copyright information.
#ifndef PIOS_KERN_MASTER_H
#define PIOS_KERN_MASTER_H

#include <inc/arch/gcc.h>

// Called on each processor to initialize the kernel.
void init(void);

// First function run in user mode (only on one processor)
void user(void);

// Called when there is no more work left to do in the system.
// The grading scripts trap calls to this to know when to stop.
void done(void) gcc_noreturn;

#endif
