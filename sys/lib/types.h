#ifndef _LIB_TYPES_H_
#define _LIB_TYPES_H_

typedef signed char		int8_t;
typedef unsigned char		uint8_t;
typedef short			int16_t;
typedef unsigned short		uint16_t;
typedef int			int32_t;
typedef unsigned int		uint32_t;
typedef long long		int64_t;
typedef unsigned long long	uint64_t;

typedef uint32_t		uintptr_t;
typedef int32_t			intptr_t;
typedef uint32_t		size_t;
typedef int32_t			ssize_t;

typedef uint8_t			bool;
#define TRUE			((bool) 1)
#define FALSE			((bool) 0)

#define NULL			((void *) 0)

typedef enum {
	SZ8, SZ16, SZ32
} data_sz_t;

uint32_t max(uint32_t a, uint32_t b);
uint32_t min(uint32_t a, uint32_t b);
uint32_t rounddown(uint32_t a, uint32_t n);
uint32_t roundup(uint32_t b, uint32_t n);

#endif /* !_LIB_TYPES_H_ */
