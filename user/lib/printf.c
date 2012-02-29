/*
 * Derived from PIOS-SMPC.
 */

/*
 * Implementation of cprintf console output for user environments,
 * based on printfmt() and cputs().
 *
 * cprintf is a debugging facility, not a generic output facility.
 * It is very important that it always go to the console, especially when
 * debugging file descriptor code!
 *
 * Copyright (c) 1986, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * See section "BSD License" in the file LICENSES for licensing terms.
 *
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Adapted for PIOS by Bryan Ford at Yale University.
 */

#include <stdarg.h>
#include <stdio.h>
#include <syscall.h>
#include <types.h>

#define CPUTS_MAX	256

// Collect up to CPUTS_MAX-1 characters into a buffer
// and perform ONE system call to print all of them,
// in order to make the lines output to the console atomic
// and prevent interrupts from causing context switches
// in the middle of a console output line and such.
struct printbuf {
	int idx;	// current buffer index
	int cnt;	// total bytes printed so far
	char buf[CPUTS_MAX];
};

static void
putch(int ch, struct printbuf *b)
{
	b->buf[b->idx++] = ch;
	if (b->idx == CPUTS_MAX-1) {
		b->buf[b->idx] = 0;
		cputs(b->buf);
		b->idx = 0;
	}
	b->cnt++;
}

void
cputs(const char *str)
{
	const char *c = str;

	while (*c != '\0') {
		sys_putc(c);
		c++;
	}
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
printf(const char *fmt, ...)
{
	va_list ap;
	int cnt;

	va_start(ap, fmt);
	cnt = vcprintf(fmt, ap);
	va_end(ap);

	return cnt;
}

void
gets(char* buf, int size)
{
	int num;
	char c;

	num = 0;

	while (num < size - 1) {
		c = 0;
		while ((c = sys_getc()) == 0);
		if (c >= ' ') {
			sys_putc(&c);
			buf [num++] = c;
		}

		if (c == '\n' || c == '\r') {
			char ch = '\n';
			sys_putc(&ch);
			buf[num] = 0;
			return;
		}

		if (c == '\b' && num > 0) {
			char ch = '\b';
			num--;
			sys_putc(&ch);
		}
	}

	buf[size-1]=0;

	return;
}
