#include <sys/types.h>
#include <sys/x86.h>

void
ccomp_rdtscp(uint64_t *tsc)
{
	*tsc = rdtscp();
}
