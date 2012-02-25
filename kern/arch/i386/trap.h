#ifndef _KERN_TRAP_H_
#define _KERN_TRAP_H_

#include <architecture/types.h>
#include <architecture/x86.h>

#include <kern/hvm/vmm.h>

typedef
struct trapframe {
	/* registers and other info we push manually in trapasm.S */
	pushregs tf_regs;
	uint16_t tf_es;		uint16_t padding_es;
	uint16_t tf_ds;		uint16_t padding_ds;
	uint32_t tf_trapno;

	/* format from here on determined by x86 hardware architecture */
	uint32_t tf_err;
	uintptr_t tf_eip;
	uint16_t tf_cs;		uint16_t padding_cs;
	uint32_t tf_eflags;

	/* rest included only when crossing rings, e.g., user to kernel */
	uintptr_t tf_esp;
	uint16_t tf_ss;		uint16_t padding_ss;
} trapframe;

typedef void (*kern_trap_handler_t)(struct vm *, struct trapframe *);

void trap(trapframe *) gcc_noreturn;
void trap_return(trapframe *) gcc_noreturn;

void trap_register_default_kern_handler(kern_trap_handler_t);
void trap_register_kern_handler(int trapno, kern_trap_handler_t);


/*
 * TODO: add interface to register trap handler for traps from userspace
 * TODO: remove the similar registeration service from context module
 */

#endif /* !_KERN_TRAP_H_ */
