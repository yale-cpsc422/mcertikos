#include <sys/types.h>

#include <dev/tsc.h>

uint64_t *
ccomp_tsc_per_ms(void)
{
	return (uint64_t *) &tsc_per_ms;
}
