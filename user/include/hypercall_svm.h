#ifndef _USER_HYPERCALL_SVM_H_
#define _USER_HYPERCALL_SVM_H_

#define gcc_inline	__inline __attribute__((always_inline))
typedef unsigned int	uint32_t;

typedef enum {
	HYPERCALL_BITAND,
	HYPERCALL_BITOR,
	HYPERCALL_BITXOR,
	HYPERCALL_BITNOT,
	HYPERCALL_GETC,
} hypercall_t;

/*
 * CertiKOS hypercall calling convention
 * - Input
 *    %eax/%rax: hypercall number (defined in hypercall_t)
 *    %ebx/%rbx: the 1st parameter
 *    %ecx/%rcx: the 2st parameter
 *    %edx/%rdx: the 3rd parameter
 *    %esi/%rsi: the 4th parameter
 * - Output
 *    %eax/%rax: the retturn value
 *    others   : unmodified
 */

/* helper macros*/
#define DEF_HYPERCALL_HEAD(name) static uint32_t gcc_inline hypercall_##name
#define DEF_HYPERCALL_BODY(nr)			\
	{					\
	uint32_t c;				\
	asm volatile("vmmcall"			\
	: "=a" (c)				\
	: "a" (nr)
#define DEF_HYPERCALL_RET			\
	: "cc", "memory");			\
	return c;				\
	}
#define DEF_HYPERCALL_0(name, nr)		\
	DEF_HYPERCALL_HEAD(name)(void)		\
	DEF_HYPERCALL_BODY(nr)			\
	DEF_HYPERCALL_RET
#define DEF_HYPERCALL_1(name, nr)		\
	DEF_HYPERCALL_HEAD(name)(uint32_t a0)	\
	DEF_HYPERCALL_BODY(nr)			\
	, "b" (a0)				\
	DEF_HYPERCALL_RET
#define DEF_HYPERCALL_2(name, nr)				\
	DEF_HYPERCALL_HEAD(name)(uint32_t a0, uint32_t a1)	\
	DEF_HYPERCALL_BODY(nr)					\
	, "b" (a0), "c" (a1)					\
	DEF_HYPERCALL_RET
#define DEF_HYPERCALL_3(name, nr)					\
	DEF_HYPERCALL_HEAD(name)(uint32_t a0, uint32_t a1, uint32_t a2)	\
	DEF_HYPERCALL_BODY(nr)						\
	, "b" (a0), "c" (a1), "d" (a2)					\
	DEF_HYPERCALL_RET
#define DEF_HYPERCALL_4(name, nr)					\
	DEF_HYPERCALL_HEAD(name)(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) \
	DEF_HYPERCALL_BODY(nr)						\
	, "b" (a0), "c" (a1), "d" (a2), "S" (a3)			\
	DEF_HYPERCALL_RET

/*
 * Define hypercalls.
 *
 * DEF_HYPERCALL_#(name, nr) defines such a function
 * - it's for the hypercall nr (defined as hypercall_t)
 * - its name is hypercall_##name
 * - it requires # parameters
 */
DEF_HYPERCALL_2(bitand, HYPERCALL_BITAND)
DEF_HYPERCALL_2(bitor, HYPERCALL_BITOR)
DEF_HYPERCALL_2(bitxor, HYPERCALL_BITXOR)
DEF_HYPERCALL_1(bitnot, HYPERCALL_BITNOT)
DEF_HYPERCALL_0(getc, HYPERCALL_GETC)

#endif /* _USER_HYPERCALL_SVM_H_ */
