#ifndef PIOS_INC_STRING_H
#define PIOS_INC_STRING_H

#include <inc/arch/types.h>

int	strlen(const char *s);
int	strnlen(const char *s, size_t size);
char *	strcpy(char *dst, const char *src);
char *	strncpy(char *dst, const char *src, size_t size);
size_t	strlcpy(char *dst, const char *src, size_t size);
int	strcmp(const char *s1, const char *s2);
int	strncmp(const char *s1, const char *s2, size_t size);
char *	strchr(const char *s, char c);
char *	strfind(const char *s, char c);
long	strtol(const char *s, char **endptr, int base);

char *	strerror(int err);

#endif /* not PIOS_INC_STRING_H */
