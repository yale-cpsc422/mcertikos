#ifndef _KERN_CONTEXT_H_
#define _KERN_CONTEXT_H_

#ifdef _KERN_

#include <lib/seg.h>
#include <lib/trap.h>
#include <lib/types.h>

#include <mm/export.h>

struct context {
	tf_t 		tf;	/* trapframe */
};

void ctx_init(struct context *, uintptr_t entry, uintptr_t stack_top);

void ctx_start(struct context *);

uint32_t ctx_errno(struct context *);
uint32_t ctx_arg1(struct context *);
uint32_t ctx_arg2(struct context *);
uint32_t ctx_arg3(struct context *);
uint32_t ctx_arg4(struct context *);
uint32_t ctx_arg5(struct context *);
uint32_t ctx_arg6(struct context *);
void ctx_set_errno(struct context *, uint32_t);
void ctx_set_retval1(struct context *, uint32_t);
void ctx_set_retval2(struct context *, uint32_t);
void ctx_set_retval3(struct context *, uint32_t);
void ctx_set_retval4(struct context *, uint32_t);
void ctx_set_retval5(struct context *, uint32_t);

#endif /* _KERN_ */

#endif /* !_KERN_CONTEXT_H_ */
