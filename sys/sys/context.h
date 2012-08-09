#ifndef _KERN_CONTEXT_H_
#define _KERN_CONTEXT_H_

#ifdef _KERN_

#include <sys/gcc.h>
#include <sys/types.h>

#include <machine/trap.h>

struct proc;	/* defined in sys/sys/proc.h */

struct context {
	tf_t 		tf;	/* trapframe */

	struct proc	*p;	/* must be NULL if it's not the context of a
				   process */
};

typedef uint32_t (*ctx_cb_t) (struct context *);

void ctx_init(struct proc *, void (*entry)(void), uintptr_t stack);

void ctx_start(struct context *) gcc_noreturn;

uint32_t ctx_errno(struct context *);
uint32_t ctx_arg1(struct context *);
uint32_t ctx_arg2(struct context *);
uint32_t ctx_arg3(struct context *);
uint32_t ctx_arg4(struct context *);
void ctx_set_retval(struct context *, uint32_t);

void ctx_dump(struct context *);

#endif /* _KERN_ */

#endif /* !_KERN_CONTEXT_H_ */
