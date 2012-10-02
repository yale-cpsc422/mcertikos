#ifndef _USER_X86_H_
#define _USER_X86_H_

#include <gcc.h>
#include <types.h>

static gcc_inline void
smp_wmb(void)
{
	__asm __volatile("":::"memory");
}

static gcc_inline void
smp_rmb(void)
{
	__asm __volatile("":::"memory");
}

#endif /* !_USER_X86_H_ */
