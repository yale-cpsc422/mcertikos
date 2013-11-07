#include <lib/string.h>
#include <lib/types.h>

#include "console.h"
#include "serial.h"

struct {
	char buf[CONSOLE_BUFFER_SIZE];
	uint32_t rpos, wpos;
} cons;

void
cons_init()
{
	memset(&cons, 0x0, sizeof(cons));
	serial_init();
}

void
cons_intr(int (*proc)(void))
{
	int c;

	while ((c = (*proc)()) != -1) {
		if (c == 0)
			continue;
		cons.buf[cons.wpos++] = c;
		if (cons.wpos == CONSOLE_BUFFER_SIZE)
			cons.wpos = 0;
	}
}

void
cons_putc(char c)
{
	serial_putc(c);
}
