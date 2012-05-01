#ifndef _USER_HYPERCALL_SVM_H_
#define _USER_HYPERCALL_SVM_H_

#include <gcc.h>
#include <types.h>

typedef enum {
	HYPERCALL_BITAND,
	HYPERCALL_BITOR,
	HYPERCALL_BITXOR,
	HYPERCALL_BITNOT,
	HYPERCALL_GETC,
} hypercall_t;

/*
 * CertiKOS hypercall format
 * - Input
 *    %eax/%rax: hypercall number (defined in hypercall_t)
 *    %ebx/%rbx: the 1st parameter
 *    %ecx/%rcx: the 2st parameter
 *    %edx/%rdx: the 3rd parameter
 *    %esi/%rsi: the 4th parameter
 * - Output
 *    If the number of the parameters of a hypercall is less than 4, the first
 *    unused parameter register are used to store the return value. Otherwise,
 *    the last parameter register, i.e. %esi/%rsi, is used to store the return
 *    value.
 *    If the hypercall succeeds, %eax/%rax is set to 0; otherwise, it's set to
 *    1.
 */

static uint32_t gcc_inline
hypercall_bitand(uint32_t a, uint32_t b)
{
	uint32_t c;

	asm volatile("vmmcall"
		     : "=d" (c)
		     : "a" (HYPERCALL_BITAND),
		       "b" (a),
		       "c" (b)
		     : "cc", "memory");

	return c;
}

static uint32_t gcc_inline
hypercall_bitor(uint32_t a, uint32_t b)
{
	uint32_t c;

	asm volatile("vmmcall"
		     : "=d" (c)
		     : "a" (HYPERCALL_BITOR),
		       "b" (a),
		       "c" (b)
		     : "cc", "memory");

	return c;
}

static uint32_t gcc_inline
hypercall_bitxor(uint32_t a, uint32_t b)
{
	uint32_t c;

	asm volatile("vmmcall"
		     : "=d" (c)
		     : "a" (HYPERCALL_BITXOR),
		       "b" (a),
		       "c" (b)
		     : "cc", "memory");

	return c;
}

static uint32_t gcc_inline
hypercall_bitnot(uint32_t a)
{
	uint32_t c;

	asm volatile("vmmcall"
		     : "=c" (c)
		     : "a" (HYPERCALL_BITNOT),
		       "b" (a)
		     : "cc", "memory");

	return c;
}

static char gcc_inline
hypercall_getc(void)
{
	char c;

	asm volatile("vmmcall"
		     : "=b" (c)
		     : "a" (HYPERCALL_GETC)
		     : "cc", "memory");

	return c;
}

#endif /* _USER_HYPERCALL_SVM_H_ */
