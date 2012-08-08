#ifndef _KERN_STRING_H_
#define _KERN_STRING_H_

#ifndef _KERN_
#error "This is a kernel header file; do not include it in userspace program."
#endif /* !_KERN_ */

#include <sys/types.h>

int     strlen(const char *s);
int     strnlen(const char *s, size_t size);
char *  strcpy(char *dst, const char *src);
char *  strncpy(char *dst, const char *src, size_t size);
size_t  strlcpy(char *dst, const char *src, size_t size);
int     strcmp(const char *s1, const char *s2);
int     strncmp(const char *s1, const char *s2, size_t size);
char *  strchr(const char *s, char c);
char *  strfind(const char *s, char c);
long    strtol(const char *s, char **endptr, int base);
char *  strerror(int err);

void *	memset(void *dst, int c, size_t len);
void *	memzero(void *dst, size_t len);
void *	memcpy(void *dst, const void *src, size_t len);
void *	memmove(void *dst, const void *src, size_t len);
int	memcmp(const void *s1, const void *s2, size_t len);

#endif /* !_KERN_STRING_H_ */
