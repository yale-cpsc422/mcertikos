#ifndef _KERN_TYPES_H_
#define _KERN_TYPES_H_

#ifdef _KERN_

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

typedef __reg_t		reg_t;

typedef int32_t		pid_t;

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

/* Return the offset of 'member' relative to the beginning of a struct type */
#define offsetof(type, member)		__builtin_offsetof(type, member)

typedef uint8_t		bool;

#define TRUE		((bool) 1)
#define FALSE		((bool) 0)

#define NULL		0

/*
 * Types for VMM
 */

typedef int32_t vmid_t;
typedef int32_t vid_t;
typedef int32_t sid_t;

typedef enum data_sz_t {
	SZ8, 	/* 1 byte */
	SZ16, 	/* 2 byte */
	SZ32	/* 4 byte */
} data_sz_t;

#endif /* _KERN_ */

#endif /* !_KERN_TYPES_H_ */
