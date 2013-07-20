#ifndef _LIB_GCC_H_
#define _LIB_GCC_H_

#define gcc_aligned(mult)	__attribute__((aligned (mult)))

#define gcc_packed		__attribute__((packed))

#ifndef __COMPCERT__
#define gcc_inline		__inline __attribute__((always_inline))
#else
#define gcc_inline		inline
#endif

#define gcc_noinline		__attribute__((noinline))

#define gcc_noreturn		__attribute__((noreturn))

#ifndef __COMPCERT__

#define likely(x)		__builtin_expect (!!(x), 1)
#define unlikely(x)		__builtin_expect (!!(x), 0)

#else /* !__COMPCERT__ */

#define likely(x)		(x)
#define unlikely(x)		(x)

#endif /* __COMPCERT__ */

#endif /* !_LIB_GCC_H_ */
