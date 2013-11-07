#ifndef _KERN_KCTX_H_
#define _KERN_KCTX_H_

#ifdef _KERN_

#include <lib/types.h>

#define MAX_KCTX	64

struct kctx {
	uint32_t	esp;
	uint32_t	edi;
	uint32_t	esi;
	uint32_t	ebx;
	uint32_t	ebp;
	uint32_t	eip;
	bool		inuse;
};

void cswitch(struct kctx *from, struct kctx *to);

/*
 * Initialize the kernel context module.
 */
void kctx_init(void);

/*
 * Create a new kernel context.
 *
 * @param f         the initial value of eip in the kernel context
 * @param stack_top the top address of the stack
 *
 * @return a pointer to the kernel context structure if successful; otherwise,
 *         return NULL
 */
struct kctx *kctx_new(void (*f)(void), uintptr_t stack_top);

/*
 * Free a kernel context.
 *
 * @param kctx the kernel context to free
 *
 * @return 0 if successful; otherwise, return a non-zero value
 */
int kctx_free(struct kctx *kctx);

#endif /* _KERN_ */

#endif /* !_KERN_KCTX_H_ */
