#ifndef _KERN_CONTEXT_H_
#define _KERN_CONTEXT_H_

#ifdef _KERN_

#include <sys/gcc.h>
#include <sys/mmu.h>
/* #include <sys/pcpu.h> */
#include <sys/trap.h>
#include <sys/types.h>

typedef
struct context_t {
	uint8_t stack[PAGE_SIZE - sizeof(tf_t)];
	tf_t tf;
} context_t;

typedef uint32_t (*callback_t) (context_t *);

void context_init(void);

context_t *context_new(void (*f)(void), uint32_t);
void context_destroy(context_t *);

void context_register_handler(int, callback_t);

void context_start(context_t *) gcc_noreturn;

uint32_t context_errno(context_t *);
uint32_t context_arg1(context_t *);
uint32_t context_arg2(context_t *);
uint32_t context_arg3(context_t *);
uint32_t context_arg4(context_t *);

context_t *context_cur(void);
void context_set_cur(context_t *);

#endif /* _KERN_ */

#endif /* !_KERN_CONTEXT_H_ */
