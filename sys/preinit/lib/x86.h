#ifndef _PREINIT_LIB_X86_H_
#define _PREINIT_LIB_X86_H_

#ifdef _KERN_

#include <preinit/lib/gcc.h>
#include <preinit/lib/types.h>
#include <preinit/lib/string.h>
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

static gcc_inline void
cpuid(uint32_t info,
      uint32_t *eaxp, uint32_t *ebxp, uint32_t *ecxp, uint32_t *edxp)
{
	uint32_t eax, ebx, ecx, edx;
	__asm __volatile("cpuid"
			 : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
			 : "a" (info));
	if (eaxp)
		*eaxp = eax;
	if (ebxp)
		*ebxp = ebx;
	if (ecxp)
		*ecxp = ecx;
	if (edxp)
		*edxp = edx;
}

static gcc_inline cpu_vendor
vendor()
{
    uint32_t eax, ebx, ecx, edx;
    char cpuvendor[13];

    cpuid(0x0, &eax, &ebx, &ecx, &edx);
    ((uint32_t *) cpuvendor)[0] = ebx;
    ((uint32_t *) cpuvendor)[1] = edx;
    ((uint32_t *) cpuvendor)[2] = ecx;
    cpuvendor[12] = '\0';

    if (strncmp(cpuvendor, "GenuineIntel", 20) == 0)
        return INTEL;
    else if (strncmp(cpuvendor, "AuthenticAMD", 20) == 0)
        return AMD;
    else
        return UNKNOWN_CPU;
}

static gcc_inline uint32_t
rcr3(void)
{
    uint32_t val;
    __asm __volatile("movl %%cr3,%0" : "=r" (val));
    return val;
}

#endif /* _KERN_ */

#endif /* !_PREINIT_LIB_X86_H_ */
