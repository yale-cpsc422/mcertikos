#include <sys/debug.h>
#include <sys/stdarg.h>
#include <sys/types.h>
#include <sys/x86.h>

void
debug_info(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vcprintf(fmt, ap);
	va_end(ap);
}

void
debug_normal(const char *file, int line, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	cprintf("[D] %s:%d: ", file, line);
	vcprintf(fmt, ap);
	va_end(ap);
}

void
debug_panic(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	/* /\* Avoid infinite recursion if we're panicking from kernel mode. *\/ */
	/* if ((read_cs() & 3) == 0) */
	/*	goto dead; */

	/* First print the requested message */
	va_start(ap, fmt);
	cprintf("[P] %s:%d: ", file, line);
	vcprintf(fmt, ap);
	va_end(ap);
	halt();
}

void
debug_warn(const char *file, int line, const char *fmt,...)
{
	va_list ap;
	va_start(ap, fmt);
	cprintf("[W] %s:%d: ", file, line);
	vcprintf(fmt, ap);
	va_end(ap);
}
