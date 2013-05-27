#include <sys/types.h>

#include <dev/tsc.h>

uint32_t
ccomp_tsc_freq_lo(void)
{
	return (tsc_per_ms * 1000) & 0xffffffff;
}

uint32_t
ccomp_tsc_freq_hi(void)
{
	return ((tsc_per_ms * 1000) >> 32) & 0xffffffff;
}
