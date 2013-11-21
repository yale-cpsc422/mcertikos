#ifndef _PREINIT_LIB_X86_H_
#define _PREINIT_LIB_X86_H_

#ifdef _KERN_

#include <preinit/lib/gcc.h>
#include <preinit/lib/types.h>
#include <lib/x86.h>

static gcc_inline uint32_t
read_ebp(void)
{
	uint32_t ebp;
	__asm __volatile("movl %%ebp,%0" : "=rm" (ebp));
	return ebp;
}

static gcc_inline void
lldt(uint16_t sel)
{
	__asm __volatile("lldt %0" : : "r" (sel));
}

static gcc_inline void
cli(void)
{
	__asm __volatile("cli":::"memory");
}

static gcc_inline void
sti(void)
{
	__asm __volatile("sti;nop");
}

static gcc_inline uint64_t
rdmsr(uint32_t msr)
{
	uint64_t rv;
	__asm __volatile("rdmsr" : "=A" (rv) : "c" (msr));
	return rv;
}

static gcc_inline void
wrmsr(uint32_t msr, uint64_t newval)
{
        __asm __volatile("wrmsr" : : "A" (newval), "c" (msr));
}

static gcc_inline void
halt(void)
{
	__asm __volatile("hlt");
}

static gcc_inline uint64_t
rdtsc(void)
{
	uint64_t rv;

	__asm __volatile("rdtsc" : "=A" (rv));
	return (rv);
}

static gcc_inline void
enable_sse(void)
{
	uint32_t cr0, cr4;

	cr4 = rcr4() | CR4_OSFXSR | CR4_OSXMMEXCPT;
	lcr4(cr4);

	cr0 = rcr0() | CR0_MP;
	cr0 &= ~ (CR0_EM | CR0_TS);
}

#endif /* _KERN_ */

#endif /* !_PREINIT_LIB_X86_H_ */
