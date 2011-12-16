#ifndef PIOS_INC_STDIO_H
#define PIOS_INC_STDIO_H

#include <architecture/types.h>
#include <inc/stdarg.h>

#ifndef NULL
#define NULL	((void *) 0)
#endif /* !NULL */

// kern/debug/console
int cons_getc(void);
int wait_kbd();

#define putchar(c)	fputc(c, stdout)
#define putc(c,fh)	fputc(c, fh)
#define getchar()	cons_getc()
#define getc()	cons_getc()

// lib/printfmt.c
void	printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...);
void	vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list);
int	sprintf(char *str, const char *fmt, ...);
int	vsprintf(char *str, const char *fmt, va_list);
int	snprintf(char *str, int size, const char *fmt, ...);
int	vsnprintf(char *str, int size, const char *fmt, va_list);

// lib/cputs.c (user space impl) or kern/console.c (kernel impl)
void	cputs(const char *str);

// lib/cprintf.c
int	cprintf(const char *fmt, ...);
int	vcprintf(const char *fmt, va_list);

#endif /* !PIOS_INC_STDIO_H */
