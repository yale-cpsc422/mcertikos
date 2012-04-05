#include <sys/debug.h>
#include <sys/stdarg.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>

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
#ifdef DEBUG_MSG
	va_list ap;
	va_start(ap, fmt);
	if (vmm_cur_vm() != NULL)
		dprintf("[D] %s:%d: ", file, line);
	else
		dprintf("<D> %s:%d: ", file, line);
	vdprintf(fmt, ap);
	va_end(ap);
#endif
}

void
debug_panic(const char *file, int line, const char *fmt,...)
{
#ifdef DEBUG_MSG
	va_list ap;

	va_start(ap, fmt);
	if (vmm_cur_vm() != NULL)
		cprintf("[P] %s:%d: ", file, line);
	else
		cprintf("<P> %s:%d: ", file, line);
	vcprintf(fmt, ap);
	va_end(ap);
#endif
	halt();
}

void
debug_warn(const char *file, int line, const char *fmt,...)
{
#ifdef DEBUG_MSG
	va_list ap;
	va_start(ap, fmt);
	if (vmm_cur_vm() != NULL)
		cprintf("[W] %s:%d: ", file, line);
	else
		cprintf("<W> %s:%d: ", file, line);
	vcprintf(fmt, ap);
	va_end(ap);
#endif
}
