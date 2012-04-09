#ifndef _USER_STDIO_H_
#define _USER_STDIO_H_

#include <stdarg.h>

#define MAX_BUF 4096

#define getchar()       sys_getc()
#define getc()       sys_getc()

void printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...);
void vprintfmt(void (*putch)(int, void*), void *putdat,
	       const char *fmt, va_list ap);

void cputs(const char *str);
int printf(const char *fmt, ...);
int vcprintf(const char *fmt, va_list ap);


void gets(char *, int);
void puts(const char *str);


#endif /* _USER_STDIO_H_ */
