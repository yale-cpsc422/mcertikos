#include <lib/types.h>

uint32_t
ccomp_min(uint32_t a, uint32_t b)
{
	return MIN(a, b);
}

uint32_t
ccomp_max(uint32_t a, uint32_t b)
{
	return MAX(a, b);
}

uint32_t
ccomp_rounddown(uint32_t a, size_t n)
{
	return ROUNDDOWN(a, n);
}

uint32_t
ccomp_roundup(uint32_t a, size_t n)
{
	return ROUNDUP(a, n);
}
