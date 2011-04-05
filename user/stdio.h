#ifndef PIOS_INC_STDIO_H
#define PIOS_INC_STDIO_H

#include <inc/arch/types.h>
#include <inc/arch/stdarg.h>

#ifndef NULL
#define NULL	((void *) 0)
#endif /* !NULL */

#define MAX_BUF 4096

#define getchar()	getc()

// lib/printfmt.c
void	printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...);
void	vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list);
int	sprintf(char *str, const char *fmt, ...);
int	vsprintf(char *str, const char *fmt, va_list);
int	snprintf(char *str, int size, const char *fmt, ...);
int	vsnprintf(char *str, int size, const char *fmt, va_list);

// lib/cputs.c (user space impl) or kern/console.c (kernel impl)
void	puts(const char *str);
void	gets(char *str, int size);

// lib/cprintf.c
void	putc(char c);
int	printf(const char *fmt, ...);
int	vprintf(const char *fmt, va_list);

int atoi(const char* buf, int* i);

#endif /* !PIOS_INC_STDIO_H */
