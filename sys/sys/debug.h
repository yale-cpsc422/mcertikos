#ifndef _KERN_DEBUG_H_
#define _KERN_DEBUG_H_

#ifndef _KERN_
#error "This is a kernel header file; do not include it in userspace program."
#endif /* !_KERN_ */

#include <sys/console.h>
#include <sys/gcc.h>
#include <sys/stdarg.h>

#define KERN_INFO(...)		debug_info(__VA_ARGS__)

#define KERN_DEBUG(...)		debug_normal(__FILE__, __LINE__, __VA_ARGS__)

#define KERN_WARN(...)		debug_warn(__FILE__, __LINE__, __VA_ARGS__)

#define KERN_PANIC(...)		debug_panic(__FILE__, __LINE__, __VA_ARGS__)

#define KERN_ASSERT(x)							\
	do {								\
		if (!(x))						\
			KERN_PANIC("Kernel assertion failed: %s\n", #x); \
	} while(0)

#define getchar()	cons_getc()

int vcprintf(const char *, va_list);
int cprintf(const char *, ...);
int vdprintf(const char *, va_list);
int dprintf(const char *, ...);
void vprintfmt(void (*putch)(int, void *), void *, const char *, va_list);

void debug_info(const char *, ...);
void debug_normal(const char *, int, const char *, ...);
void debug_warn(const char*, int, const char*, ...);
void debug_panic(const char*, int, const char*, ...);

#endif /* !_KERN_DEBUG_H_ */
