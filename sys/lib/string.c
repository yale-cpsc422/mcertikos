#include "string.h"
#include "types.h"

int
strncmp(const char *p, const char *q, size_t n)
{
	while (n > 0 && *p && *p == *q)
		n--, p++, q++;
	if (n == 0)
		return 0;
	else
		return (int) ((unsigned char) *p - (unsigned char) *q);
}

int
strnlen(const char *s, size_t size)
{
	int n;

	for (n = 0; size > 0 && *s != '\0'; s++, size--)
		n++;
	return n;
}

void *
memset(void *v, int c, size_t n)
{
	char *ptr = v;
	while (n--)
		ptr[n] = c;
	return v;
}

void *
memzero(void *v, size_t n)
{
	return memset(v, 0, n);
}

void *
memmove(void *dst, const void *src, size_t n)
{
	const char *s;
	char *d;

	s = src;
	d = dst;
	if (s < d && s + n > d) {
		while (n--)
			d[n] = s[n];
	} else {
		int i;
		for (i = 0; i < n; i++)
			d[i] = s[i];
	}

	return dst;
}

void *
memcpy(void *dst, const void *src, size_t n)
{
	return memmove(dst, src, n);
}
