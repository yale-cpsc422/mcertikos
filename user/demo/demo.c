#include <hypercall_svm.h>
#include <stdio.h>

static char *
demo_gets(char *s, int size)
{
	int i;
	char *p;

	if (s == 0 || size <= 0)
		return 0;

	i = size;
	p = s;

	while (i > 1 && (*p = hypercall_getc()) != '\n') {
		i--;
		p++;
	}
	*p = '\0';

	return s;
}

int
main(int argc, char *argv)
{
	char buf[4096];

	demo_gets(buf, 4096);
	printf("%s\n", buf);

	return 0;
}
