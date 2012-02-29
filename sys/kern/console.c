#include <sys/console.h>
#include <sys/debug.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>

#include <dev/kbd.h>
#include <dev/serial.h>
#include <dev/video.h>

struct {
	spinlock_t lk;

	char buf[CONSOLE_BUFFER_SIZE];
	uint32_t rpos, wpos;
	bool kbd_enabled;
} cons;

void
cons_init()
{
	memset(&cons, 0x0, sizeof(cons));

	spinlock_init(&cons.lk);

	cons.kbd_enabled = FALSE;

	serial_init();
	video_init();
	kbd_init();
}

void
cons_intr(int (*proc)(void))
{
	int c;

	spinlock_acquire(&cons.lk);

	while ((c = (*proc)()) != -1) {
		if (c == 0)
			continue;
		cons.buf[cons.wpos++] = c;
		if (cons.wpos == CONSOLE_BUFFER_SIZE)
			cons.wpos = 0;
	}

	spinlock_release(&cons.lk);
}

int
cons_getc(void)
{
	int c;

	// poll for any pending input characters,
	// so that this function works even when interrupts are disabled
	// (e.g., when called from the kernel monitor).
	serial_intr();
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
	serial_putc(c);
	video_putc(c);
}
