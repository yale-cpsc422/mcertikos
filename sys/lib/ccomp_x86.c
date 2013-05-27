#include <sys/debug.h>
#include <sys/types.h>
#include <sys/x86.h>

void
ccomp_rdtscp(uint64_t *tsc)
{
	*tsc = rdtscp();
}

uint32_t
ccomp_rcr4(void)
{
	return rcr4();
}

void
ccomp_lcr4(uint32_t cr4)
{
	lcr4(cr4);
}

void
ccomp_lcr0(uint32_t cr0)
{
	lcr0(cr0);
}

void
ccomp_lcr3(uint32_t cr3)
{
	lcr3(cr3);
}

uint32_t
ccomp_rcr0(void)
{
	return rcr0();
}

void
ccomp_enable_sse(void)
{
	uint32_t dummy, ecx, edx, cr0, cr4;

	cpuid(0x1, &dummy, &dummy, &ecx, &edx);
	if ((ecx & (CPUID_FEATURE_SSE3 |
		    CPUID_FEATURE_SSSE3 |
		    CPUID_FEATURE_SSE41 |
		    CPUID_FEATURE_SSE42)) == 0 &&
	    (edx & (CPUID_FEATURE_SSE |
		    CPUID_FEATURE_SSE2)) == 0)
		KERN_PANIC("CPU doesn't support SSE.\n");

	cr4 = rcr4() | CR4_OSFXSR | CR4_OSXMMEXCPT;
	lcr4(cr4);

	cr0 = rcr0() | CR0_MP;
	cr0 &= ~ (CR0_EM | CR0_TS);
}

void
ccomp_pause(void)
{
	pause();
}

void
ccomp_cld(void)
{
	cld();
}

uint32_t
ccomp_rcr2(void)
{
	return rcr2();
}

uint8_t
ccomp_inb(int port)
{
	return inb(port);
}

uint16_t
ccomp_inw(int port)
{
	return inw(port);
}

uint32_t
ccomp_inl(int port)
{
	return inl(port);
}

void
ccomp_outb(int port, uint8_t data)
{
	outb(port, data);
}

void
ccomp_outw(int port, uint16_t data)
{
	outw(port, data);
}

void
ccomp_outl(int port, uint32_t data)
{
	outl(port, data);
}
