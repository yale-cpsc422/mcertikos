// Per-CPU kernel state.
// See COPYRIGHT for copyright information.
#ifndef PIOS_KERN_CONTEXT_H
#define PIOS_KERN_CONTEXT_H

#include <inc/arch/types.h>
#include <inc/arch/mmu.h>

#ifndef __ASSEMBLER

// context is an abstract type
//typedef void context;
typedef struct context_opaque {
   char stuff[PAGESIZE];
} context;

typedef struct kstack_opaque {
   char stuff[PAGESIZE];
} kstack;

typedef uint32_t (*callback) (context *ctx);

// activates the context subsystem
// fills out certain data structures
// This should be done once per memory
void context_init();

// kstack_init:
// updates a simple active kernel stack into
// an advanced kernel stack
// adds support for context activation, etc
void kstack_init();


context* context_new(void (*f)(void), uint32_t expected_va);
void context_destroy(context* ctx);
void context_handler(int trapno, callback func);
void gcc_noreturn context_start (context *ctx);

uint32_t context_errno (context* ctx);
uint32_t context_arg1(context* ctx);
uint32_t context_arg2(context* ctx);
uint32_t context_arg3(context* ctx);
uint32_t context_arg4(context* ctx);

// DEBUG
void context_debug(context* ctx);

#endif // !_ASSEMBLER
#endif // PIOS_KERN_CPU_H
