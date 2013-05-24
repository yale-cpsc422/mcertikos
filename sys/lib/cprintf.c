/*
 * Implementation of cprintf console output for user environments,
 * based on printfmt() and cputs().
 *
 * cprintf is a debugging facility, not a generic output facility.
 * It is very important that it always go to the console, especially when
 * debugging file descriptor code!
 */

#include <sys/debug.h>
#include <sys/stdarg.h>
#include <sys/types.h>

#include <dev/cons.h>

struct printbuf {
	int idx;	/* current buffer index */
	int cnt;	/* total bytes printed so far */
	char buf[1024];
};

static void
cputs(const char *str)
{
	while (*str)
		cons_putc(0, *str++);
}

static void
putch(int ch, struct printbuf *b)
{
	b->buf[b->idx++] = ch;
	if (b->idx == 1024 - 1) {
		b->buf[b->idx] = 0;
		cputs(b->buf);
		b->idx = 0;
	}
	b->cnt++;
}

int
vcprintf(const char *fmt, va_list ap)
{
	struct printbuf b;

	b.idx = 0;
	b.cnt = 0;
	vprintfmt((void*)putch, &b, fmt, ap);

	b.buf[b.idx] = 0;
	cputs(b.buf);

	return b.cnt;
}

int
cprintf(const char *fmt, ...)
{
	va_list ap;
	int cnt;

	va_start(ap, fmt);
	cnt = vcprintf(fmt, ap);
	va_end(ap);

	return cnt;
}
