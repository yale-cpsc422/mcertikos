#ifdef DEBUG_MSG

#include <lib/debug.h>
#include <lib/stdarg.h>
#include <lib/types.h>

#include <dev/console.h>
#include <dev/serial.h>

struct dprintbuf {
	int idx;	/* current buffer index */
	int cnt;	/* total bytes printed so far */
	char buf[CONSOLE_BUFFER_SIZE];
};

static void
cputs(const char *str)
{
	while (*str)
		serial_putc(*str++);
}

static void
putch(int ch, struct dprintbuf *b)
{
	b->buf[b->idx++] = ch;
	if (b->idx == CONSOLE_BUFFER_SIZE - 1 ) {
		b->buf[b->idx] = 0;
		cputs(b->buf);
		b->idx = 0;
	}
	b->cnt++;
}

int
vdprintf(const char *fmt, va_list ap)
{
#ifdef SERIAL_DEBUG
	struct dprintbuf b;

	b.idx = 0;
	b.cnt = 0;
	vprintfmt((void*)putch, &b, fmt, ap);

	b.buf[b.idx] = 0;
	cputs(b.buf);

	return b.cnt;
#else /* !SERIAL_DEBUG */
	return vcprintf(fmt, ap);
#endif /* SERIAL_DEBUG */
}

int
dprintf(const char *fmt, ...)
{
	va_list ap;
	int cnt;

	va_start(ap, fmt);
	cnt = vdprintf(fmt, ap);
	va_end(ap);

	return cnt;
}

#endif /* DEBUG_MSG */
