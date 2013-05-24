#ifndef _KERN_DEBUG_H_
#define _KERN_DEBUG_H_

#ifdef _KERN_

#include <sys/gcc.h>
#include <sys/stdarg.h>

#ifdef DEBUG_MSG

#define KERN_DEBUG(...) do {					\
		debug_lock();					\
		debug_normal(__FILE__, __LINE__, __VA_ARGS__);	\
		debug_unlock();					\
	} while (0)

#define KERN_WARN(...) do {					\
		debug_lock();					\
		debug_warn(__FILE__, __LINE__, __VA_ARGS__);	\
		debug_unlock();					\
	} while (0)

#define KERN_PANIC(...) do {					\
		debug_lock();					\
		debug_panic(__FILE__, __LINE__, __VA_ARGS__);	\
		debug_unlock();					\
	} while (0)

#define KERN_ASSERT(x) do {						\
		if (!(x))						\
			KERN_PANIC("Kernel assertion failed: %s\n", #x); \
	} while(0)

#else /* !DEBUG_MSG */

#define KERN_DEBUG(...) do {			\
	} while (0)

#define KERN_WARN(...) do {			\
	} while (0)

#define KERN_PANIC(...) do {			\
	} while (0)

#define KERN_ASSERT(c) do {			\
	} while (0)

#endif /* DEBUG_MSG */

#define KERN_INFO(fmt, ...) do {		\
		debug_lock();			\
		debug_info(fmt, ##__VA_ARGS__);	\
		debug_unlock();			\
	} while (0)

int cprintf(const char *, ...);
int vcprintf(const char *, va_list);
void vprintfmt(void (*putch)(int, void *), void *, const char *, va_list);

void debug_init(void);
void debug_lock(void);
void debug_unlock(void);
void debug_info(const char *, ...);

#ifdef DEBUG_MSG

int vdprintf(const char *, va_list);
int dprintf(const char *, ...);

void debug_normal(const char *, int, const char *, ...);
void debug_warn(const char*, int, const char*, ...);
void debug_panic(const char*, int, const char*, ...);

#else

#define dprintf(...) do {			\
	} while (0)

#endif

#endif /* _KERN_ */

#endif /* !_KERN_DEBUG_H_ */
