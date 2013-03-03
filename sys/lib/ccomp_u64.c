#include <sys/types.h>

void
ccomp_u64_assign_var(uint64_t *a, uint64_t *b)
{
	if (a == b)
		return;
	*b = *a;
}

void
ccomp_u64_assign_val(uint32_t lo, uint32_t hi, uint64_t *b)
{
	*b = ((uint64_t) hi << 32) | lo;
}

uint32_t
ccomp_u64_lo(uint64_t *a)
{
	return (*a) & 0xffffffff;
}

uint32_t
ccomp_u64_hi(uint64_t *a)
{
	return (*a >> 32) & 0xffffffff;
}

int
ccomp_u64_cmp(uint64_t *_a, uint64_t *_b)
{
	uint64_t a = *_a, b = *_b;
	return (a == b) ? 0 : (a < b) ? 1 : 2;
}

void
ccomp_u64_add(uint64_t *a, uint64_t *b, uint64_t *c)
{
	*c = *a + *b;
}

void
ccomp_u64_sub(uint64_t *a, uint64_t *b, uint64_t *c)
{
	*c = *a - *b;
}

void
ccomp_u64_mul(uint64_t *a, uint64_t *b, uint64_t *c)
{
	*c = *a * *b;
}

void
ccomp_u64_div(uint64_t *a, uint64_t *b, uint64_t *c)
{
	*c = *a / *b;
}

void
ccomp_u64_mod(uint64_t *a, uint64_t *b, uint64_t *c)
{
	*c = *a % *b;
}
