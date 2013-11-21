#include "string.h"
#include <preinit/lib/types.h>

void *
memset(void *v, int c, size_t n)
{
	char *ptr = v;
	while (n--)
		ptr[n] = c;
	return v;
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
