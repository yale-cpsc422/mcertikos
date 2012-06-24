#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/spinlock.h>
#include <sys/stdarg.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>

static spinlock_t debug_lk;

void
debug_init(void)
{
	spinlock_init(&debug_lk);
}

void
debug_lock(void)
{
	spinlock_acquire(&debug_lk);
}

void
debug_unlock(void)
{
	spinlock_release(&debug_lk);
}

void
debug_info(const char *fmt, ...)
{
	spinlock_acquire(&debug_lk);

	va_list ap;
	va_start(ap, fmt);
	vcprintf(fmt, ap);
	va_end(ap);

	spinlock_release(&debug_lk);
}

void
debug_normal(const char *file, int line, const char *fmt, ...)
{
	spinlock_acquire(&debug_lk);

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

	spinlock_release(&debug_lk);
}

void
debug_panic(const char *file, int line, const char *fmt,...)
{
	spinlock_acquire(&debug_lk);

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
	intr_local_disable();
	spinlock_release(&debug_lk);
	halt();
}

void
debug_warn(const char *file, int line, const char *fmt,...)
{
	spinlock_acquire(&debug_lk);

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

	spinlock_release(&debug_lk);
}
