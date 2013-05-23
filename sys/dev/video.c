#include <sys/debug.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <dev/video.h>

#define MONO_BASE	0x3B4
#define MONO_BUF	0xB0000
#define CGA_BASE	0x3D4
#define CGA_BUF		0xB8000

static unsigned addr_6845;
static uint16_t *crt_buf;
static volatile uint16_t crt_pos;

void
video_init(void)
{
	volatile uint16_t *cp;
	uint16_t was;

	/* Get a pointer to the memory-mapped text display buffer. */
	cp = (uint16_t*) CGA_BUF;
	was = *cp;
	*cp = (uint16_t) 0xA55A;
	if (*cp != 0xA55A) {
		cp = (uint16_t*) MONO_BUF;
		addr_6845 = MONO_BASE;
	} else {
		*cp = was;
		addr_6845 = CGA_BASE;
	}

	/* clear the screen */
	crt_buf = (uint16_t*) cp;
	memzero(crt_buf, CRT_SIZE * sizeof(uint16_t));

	/* set cursor to (0, 0) */
	video_set_cursor(0);
}

uint16_t
video_getraw(char c)
{
	return ((c & 0xff) | 0x0700);
}

void
video_putc(int pos, char c)
{
	KERN_ASSERT(pos < CRT_SIZE);
	crt_buf[pos] = video_getraw(c);
}

void
video_scroll_up(int nlines)
{
	KERN_ASSERT(nlines <= CRT_ROWS);
	if (nlines < CRT_ROWS)
		memcpy(crt_buf, crt_buf + nlines * CRT_COLS,
		       (CRT_SIZE - nlines * CRT_COLS) * sizeof(uint16_t));
	memzero(crt_buf + CRT_SIZE - nlines * CRT_COLS,
		nlines * CRT_COLS * sizeof(uint16_t));
}

void
video_set_cursor(int pos)
{
	KERN_ASSERT(pos < CRT_SIZE);
	crt_pos = pos;
	outb(addr_6845, 14);
	outb(addr_6845 + 1, crt_pos >> 8);
	outb(addr_6845, 15);
	outb(addr_6845 + 1, crt_pos);
}

void
video_buf_write(int pos, uint16_t *src, size_t n)
{
	KERN_ASSERT(pos < CRT_SIZE);
	KERN_ASSERT(src != NULL);
	KERN_ASSERT(n <= CRT_SIZE - pos);
	memcpy(crt_buf + pos, src, n * sizeof(uint16_t));
}

void
video_buf_clear(int pos, size_t n)
{
	KERN_ASSERT(pos < CRT_SIZE);
	KERN_ASSERT(n <= CRT_SIZE - pos);
	memzero(crt_buf + pos, n);
}
