#include <lib/string.h>
#include <lib/types.h>

#include "console.h"
#include "kbd.h"
#include "serial.h"
#include "video.h"

struct {
	char buf[CONSOLE_BUFFER_SIZE];
	uint32_t rpos, wpos;
	bool kbd_enabled;
} cons;

void
cons_init()
{
	memset(&cons, 0x0, sizeof(cons));

	cons.kbd_enabled = FALSE;

	serial_init();
	video_init();
	kbd_init();
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

int
cons_getc(void)
{
	int c;

	// poll for any pending input characters,
	// so that this function works even when interrupts are disabled
	// (e.g., when called from the kernel monitor).
#ifdef SERIAL_DEBUG
	serial_intr();
#endif
	kbd_intr();

	// grab the next character from the input buffer.
	if (cons.rpos != cons.wpos) {
		c = cons.buf[cons.rpos++];
		if (cons.rpos == CONSOLE_BUFFER_SIZE)
			cons.rpos = 0;
		return c;
	}

	return 0;
}

void
cons_putc(char c)
{
#ifdef SERIAL_DEBUG
	serial_putc(c);
#endif
	video_putc(c);
}
