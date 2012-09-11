#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/pcpu.h>
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
	dprintf("[D] %s:%d: ", file, line);
	vdprintf(fmt, ap);
	va_end(ap);
#endif

	spinlock_release(&debug_lk);
}

#define DEBUG_TRACEFRAMES	10

static void
debug_trace(uintptr_t ebp, uintptr_t *eips)
{
	int i;
	uintptr_t *frame = (uintptr_t *) ebp;

	for (i = 0; i < DEBUG_TRACEFRAMES && frame; i++) {
		eips[i] = frame[1];		/* saved %eip */
		frame = (uintptr_t *) frame[0];	/* saved %ebp */
	}
	for (; i < DEBUG_TRACEFRAMES; i++)
		eips[i] = 0;
}

gcc_noinline void
debug_panic(const char *file, int line, const char *fmt,...)
{
	int i;
	uintptr_t eips[DEBUG_TRACEFRAMES];
	va_list ap;

	spinlock_acquire(&debug_lk);

	va_start(ap, fmt);
	cprintf("[P] %s:%d: ", file, line);
	vcprintf(fmt, ap);
	va_end(ap);

	debug_trace(read_ebp(), eips);
	for (i = 0; i < DEBUG_TRACEFRAMES && eips[i] != 0; i++)
		cprintf("\tfrom 0x%08x\n", eips[i]);

	cprintf("Kernel Panic on CPU%d !!!\n", pcpu_cpu_idx(pcpu_cur()));

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
	cprintf("[W] %s:%d: ", file, line);
	vcprintf(fmt, ap);
	va_end(ap);
#endif

	spinlock_release(&debug_lk);
}
