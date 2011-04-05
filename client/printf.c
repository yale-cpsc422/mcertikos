// Implementation of cprintf console output for user environments,
// based on printfmt() and cputs().
//
// cprintf is a debugging facility, not a generic output facility.
// It is very important that it always go to the console, especially when 
// debugging file descriptor code!

#include <inc/arch/gcc.h>
#include <inc/arch/types.h>
#include <inc/arch/stdarg.h>

#include <client/stdio.h>
#include <client/syscall.h>

// Collect up to MAX_BUF-1 characters into a buffer
// and perform ONE system call to print all of them,
// in order to make the lines output to the console atomic
// and prevent interrupts from causing context switches
// in the middle of a console output line and such.
struct printbuf {
	int idx;	// current buffer index
	int cnt;	// total bytes printed so far
	char buf[MAX_BUF];
};


void gets(char* buf, int size) {
	int num = 0;
	char c=0;
	char echo[2];
	echo[1]=0;
    while(num < (size-1)) {
		c=0;
		while ((c = getc()) == 0);
		echo[0] = c;
		puts(echo);
		if (c == '\n' || c == '\r') {
			buf[num] = 0;
			return;
		}
		buf [num++] = c;
	}
	buf[size-1]=0;
	return;
}	

static void
putch(int ch, struct printbuf *b)
{
	b->buf[b->idx++] = ch;
	if (b->idx == MAX_BUF-1) {
		b->buf[b->idx] = 0;
		puts(b->buf);
		b->idx = 0;
	}
	b->cnt++;
}

int
vprintf(const char *fmt, va_list ap)
{
	struct printbuf b;

	b.idx = 0;
	b.cnt = 0;
	vprintfmt((void*)putch, &b, fmt, ap);

	b.buf[b.idx] = 0;
	puts(b.buf);

	return b.cnt;
}

int
printf(const char *fmt, ...)
{
	va_list ap;
	int cnt;

	va_start(ap, fmt);
	cnt = vprintf(fmt, ap);
	va_end(ap);

	return cnt;
}

