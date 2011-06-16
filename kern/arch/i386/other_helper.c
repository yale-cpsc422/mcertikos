#include <inc/gcc.h>
#include <architecture/types.h>
#include <architecture/x86.h>
#include <architecture/mmu.h>
#include <architecture/mp.h>
#include <architecture/pic.h>
#include <architecture/mem.h>

#include <architecture/context.h>

#include <kern/debug/debug.h>
#include <kern/debug/stdio.h>

#include <kern/mem/mem.h>

// this function initializes the system on secondary processors.

void other_init(void(*f)(void)) {
	mp_donebooting();
	kstack_init();
	interrupts_init();
	f();
}
