#ifndef _USER_STDIO_H_
#define _USER_STDIO_H_

#include <stdarg.h>

void printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...);
void vprintfmt(void (*putch)(int, void*), void *putdat,
	       const char *fmt, va_list ap);

void cputs(const char *str);
int printf(const char *fmt, ...);
int vcprintf(const char *fmt, va_list ap);
void gets(char *, int);

#endif /* _USER_STDIO_H_ */
