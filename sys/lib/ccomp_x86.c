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
