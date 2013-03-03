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

#ifndef _CCOMP_

#define MIN(a, b)				\
	({					\
		typeof(a) _a = (a);		\
		typeof(b) _b = (b);		\
		_a <= _b ? _a : _b;		\
	})

#else

static gcc_inline size_t
MIN_SIZE(size_t a, size_t b)
{
	return a <= b ? a : b;
}

#endif

#define MAX(a, b)				\
	({					\
		typeof(a) _a = (a);		\
		typeof(b) _b = (b);		\
		_a >= _b ? _a : _b;		\
	})

#ifndef _CCOMP_

/* Round down to the nearest multiple of n */
#define ROUNDDOWN(a, n)				\
	({					\
		typeof(a) _a = (a);		\
		typeof(n) _n = (n);		\
		(typeof(a)) (_a - _a % _n);	\
	})

#else

static gcc_inline uintptr_t
ROUNDDOWN_PTR(uintptr_t a, uintptr_t n)
{
	return a - a % n;
}

#endif

/* Round up to the nearest multiple of n */
#define ROUNDUP(_a, _n)						\
	({							\
		typeof(_a) __a = (_a);				\
		typeof(_n) __n = (_n);				\
		(typeof(_a)) (ROUNDDOWN(__a + __n - 1, __n));	\
	})

/* Return the offset of 'member' relative to the beginning of a struct type */
#define offsetof(type, member)	__builtin_offsetof(type, member)

#endif /* !_KERN_TYPES_H_ */
