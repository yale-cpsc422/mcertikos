#ifndef _KERN_TYPES_H_
#define _KERN_TYPES_H_

#include <sys/gcc.h>

#include <machine/types.h>

typedef __uint8_t	uint8_t;
typedef __uint16_t	uint16_t;
typedef __uint32_t	uint32_t;
typedef __uint64_t	uint64_t;

typedef __int8_t	int8_t;
typedef __int16_t	int16_t;
typedef __int32_t	int32_t;
typedef __int64_t	int64_t;

typedef __intptr_t	intptr_t;
typedef __uintptr_t	uintptr_t;

typedef __size_t	size_t;
typedef __ssize_t	ssize_t;

typedef uint8_t		bool;
#define TRUE		((bool) 1)
#define FALSE		((bool) 0)

#define NULL		((void *) 0)

typedef int32_t		pid_t;
typedef int32_t		vmid_t;
typedef int32_t		vid_t;
typedef int32_t		chid_t;

typedef enum data_sz_t {
	SZ8, 	/* 1 byte */
	SZ16, 	/* 2 byte */
	SZ32	/* 4 byte */
} data_sz_t;

#ifndef __COMPCERT__

#define MIN(a, b)				\
	({					\
		typeof(a) _a = (a);		\
		typeof(b) _b = (b);		\
		_a <= _b ? _a : _b;		\
	})

#define MAX(a, b)				\
	({					\
		typeof(a) _a = (a);		\
		typeof(b) _b = (b);		\
		_a >= _b ? _a : _b;		\
	})

/* Round down to the nearest multiple of n */
#define ROUNDDOWN(a, n)				\
	({					\
		typeof(a) _a = (a);		\
		typeof(n) _n = (n);		\
		(typeof(a)) (_a - _a % _n);	\
	})

/* Round up to the nearest multiple of n */
#define ROUNDUP(_a, _n)						\
	({							\
		typeof(_a) __a = (_a);				\
		typeof(_n) __n = (_n);				\
		(typeof(_a)) (ROUNDDOWN(__a + __n - 1, __n));	\
	})

#else /* !__COMPCERT__ */

uint32_t ccomp_min(uint32_t a, uint32_t b);
uint32_t ccomp_max(uint32_t a, uint32_t b);
uint32_t ccomp_rounddown(uint32_t a, size_t n);
uint32_t ccomp_roundup(uint32_t a, size_t n);

#define MIN(a, b)	ccomp_min((a), (b))
#define MAX(a, b)	ccomp_max((a), (b))
#define ROUNDDOWN(a, n)	ccomp_rounddown((a), (n))
#define ROUNDUP(a, n)	ccomp_roundup((a), (n))

#endif /* __COMPCERT__ */

/* Return the offset of 'member' relative to the beginning of a struct type */
#define offsetof(type, member)	__builtin_offsetof(type, member)

#ifdef __COMPCERT__

void ccomp_u64_assign_var(uint64_t *a, uint64_t *b);		/* *b = *a */
void ccomp_u64_assign_val(uint32_t a_lo, uint32_t a_hi, uint64_t *b);

uint32_t ccomp_u64_lo(uint64_t *a);
uint32_t ccomp_u64_hi(uint64_t *b);

int  ccomp_u64_cmp(uint64_t *a, uint64_t *b);
void ccomp_u64_add(uint64_t *a, uint64_t *b, uint64_t *c);	/* *c = *a + *b */
void ccomp_u64_sub(uint64_t *a, uint64_t *b, uint64_t *c);	/* *c = *a - *b */
void ccomp_u64_mul(uint64_t *a, uint64_t *b, uint64_t *c);	/* *c = *a * *b */
void ccomp_u64_div(uint64_t *a, uint64_t *b, uint64_t *c);	/* *c = *a / *b */
void ccomp_u64_mod(uint64_t *a, uint64_t *b, uint64_t *c);	/* *c = *a % *b */

#define ccomp_u64_eq(a, b)	(ccomp_u64_cmp((a), (b)) == 0)
#define ccomp_u64_lt(a, b)	(ccomp_u64_cmp((a), (b)) == 1)
#define ccomp_u64_gt(a, b)	(ccomp_u64_cmp((a), (b)) == 2)
#define ccomp_u64_le(a, b)	(ccomp_u64_cmp((a), (b)) != 2)
#define ccomp_u64_ge(a, b)	(ccomp_u64_cmp((a), (b)) != 1)

#endif /* __COMPCERT__ */

#endif /* !_KERN_TYPES_H_ */
