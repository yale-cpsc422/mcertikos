// Per-CPU kernel state.
// See COPYRIGHT for copyright information.
#ifndef PIOS_KERN_SEG_H
#define PIOS_KERN_SEG_H

#include <inc/arch/types.h>

#ifndef __ASSEMBLER
// Find the CPU struct representing the current CPU.
// It always resides at the bottom of the page containing the CPU's stack.
// static cpu * cpu_cur(); 
// Returns true if we're running on the bootstrap CPU.
int cpu_onboot(); 
int cpu_number(); 

// Set up the current CPU's private register state such as GDT and TSS.
// Assumes the cpu struct for this CPU is basically initialized
// and that we're running on the cpu's correct kernel stack.
void cpu_init(void);
void cpu_activate(void);


typedef uint32_t context;
typedef uint32_t (*callback) (uint32_t errorcode, context *ctx);

// Initialize the trap-handling module and the processor's IDT.
void trap_new(context* ctx, void (*func)(void), uint32_t stack, int usermode);
void trap_clone(context *dest, context *src, void (*func)(void));
void trap_start(context *ctx) gcc_noreturn;
void trap_register(int cpu, int trapno, callback func);
void trap_debug(context* ctx);

uint32_t trap_arg1(context* ctx);
uint32_t trap_arg2(context* ctx);
uint32_t trap_arg3(context* ctx);
uint32_t trap_arg4(context* ctx);

#endif // !_ASSEMBLER
#endif // PIOS_KERN_CPU_H
