#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/trap.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <dev/cons.h>
#include <dev/cons_kbd.h>
#include <dev/kbd.h>
#include <dev/serial.h>
#include <dev/video.h>

static volatile bool cons_inited = FALSE;

#define CONS_TAB_OFFSET		4
#define CONS_ROWS		CRT_ROWS
#define CONS_COLS		CRT_COLS
#define CONS_OUTBUF_SIZE	(CRT_ROWS * CRT_COLS)
#define CONS_INBUF_SIZE		256

struct cons {
	int		ncols, nrows;

	uint16_t	outbuf[CONS_OUTBUF_SIZE];
	volatile int	outbuf_pos;	/* next input position in outbuf */
	volatile int	cur_pos;	/* next input position on screen */

	uint16_t	inbuf[CONS_INBUF_SIZE];
	int		inbuf_wptr, inbuf_rptr, inbuf_count;

	spinlock_t	cons_lk;
};

static struct cons consoles[MAX_CONS];
static volatile int fg_cons;

static spinlock_t cons_switch_lk;
static spinlock_t cons_serial_lk;

static const uint8_t normalmap[__KEY_MAX] = {
	[KEY_TIDE] = '`',
	[KEY_1] = '1', [KEY_2] = '2', [KEY_3] = '3', [KEY_4] = '4',
	[KEY_5] = '5', [KEY_6] = '6', [KEY_7] = '7', [KEY_8] = '8',
	[KEY_9] = '9', [KEY_0] = '0',
	[KEY_MINUS] = '-',      [KEY_PLUS] = '=',      [KEY_BACKSPACE] = '\b',
	[KEY_TAB] = '\t',       [KEY_L_BRACK] = '[',   [KEY_R_BRACK] = ']',
	[KEY_BACKSLASH] = '\\', [KEY_SEMICOLON] = ';', [KEY_QUOTE] = '\'',
	[KEY_ENTER] = '\n',     [KEY_COMMA] = ',',     [KEY_DOT] = '.',
	[KEY_SLASH] = '/',
	[KEY_A] = 'a', [KEY_B] = 'b', [KEY_C] = 'c', [KEY_D] = 'd',
	[KEY_E] = 'e', [KEY_F] = 'f', [KEY_G] = 'g', [KEY_H] = 'h',
	[KEY_I] = 'i', [KEY_J] = 'j', [KEY_K] = 'k', [KEY_L] = 'l',
	[KEY_M] = 'm', [KEY_N] = 'n', [KEY_O] = 'o', [KEY_P] = 'p',
	[KEY_Q] = 'q', [KEY_R] = 'r', [KEY_S] = 's', [KEY_T] = 't',
	[KEY_U] = 'u', [KEY_V] = 'v', [KEY_W] = 'w', [KEY_X] = 'x',
	[KEY_Y] = 'y', [KEY_Z] = 'z',
	[KEY_SPACE] = ' ',
	[KEY_KP_DIV] = '/',     [KEY_KP_MULT] = '*',   [KEY_KP_MINUS] = '-',
	[KEY_KP_PLUS] = '+',    [KEY_KP_DOT] = '.',    [KEY_KP_ENTER] = '\n',
	[KEY_KP_0] = '0', [KEY_KP_1] = '1', [KEY_KP_2] = '2', [KEY_KP_3] = '3',
	[KEY_KP_4] = '4', [KEY_KP_5] = '5', [KEY_KP_6] = '6', [KEY_KP_7] = '7',
	[KEY_KP_8] = '8', [KEY_KP_9] = '9',
};

static const uint8_t shiftmap[__KEY_MAX] = {
	[KEY_TIDE] = '~',
	[KEY_1] = '!', [KEY_2] = '@', [KEY_3] = '#', [KEY_4] = '$',
	[KEY_5] = '%', [KEY_6] = '^', [KEY_7] = '&', [KEY_8] = '*',
	[KEY_9] = '(', [KEY_0] = ')',
	[KEY_MINUS] = '_',      [KEY_PLUS] = '+',
	[KEY_L_BRACK] = '{',    [KEY_R_BRACK] = '}',
	[KEY_BACKSLASH] = '|',  [KEY_SEMICOLON] = ':', [KEY_QUOTE] = '\"',
	[KEY_COMMA] = '<',     [KEY_DOT] = '>',
	[KEY_SLASH] = '?',
	[KEY_A] = 'A', [KEY_B] = 'B', [KEY_C] = 'C', [KEY_D] = 'D',
	[KEY_E] = 'E', [KEY_F] = 'F', [KEY_G] = 'G', [KEY_H] = 'H',
	[KEY_I] = 'I', [KEY_J] = 'J', [KEY_K] = 'K', [KEY_L] = 'L',
	[KEY_M] = 'M', [KEY_N] = 'N', [KEY_O] = 'O', [KEY_P] = 'P',
	[KEY_Q] = 'Q', [KEY_R] = 'R', [KEY_S] = 'S', [KEY_T] = 'T',
	[KEY_U] = 'U', [KEY_V] = 'V', [KEY_W] = 'W', [KEY_X] = 'X',
	[KEY_Y] = 'Y', [KEY_Z] = 'Z',
};

static gcc_inline int
cons_inbuf_enqueue(struct cons *cons, uint16_t data)
{
	KERN_ASSERT(cons != NULL);

	if (cons->inbuf_count == CONS_INBUF_SIZE)
		return -1;

	int wptr = cons->inbuf_wptr;
	cons->inbuf[wptr] = data;
	cons->inbuf_wptr = (wptr + 1) % CONS_INBUF_SIZE;
	cons->inbuf_count++;

	return 0;
}

static int
cons_inbuf_enqueue_wrapper(void *param, uint16_t data)
{
	return cons_inbuf_enqueue((struct cons *) param, data);
}

static gcc_inline int
cons_inbuf_dequeue(struct cons *cons, uint16_t *data)
{
	KERN_ASSERT(cons != NULL);
	KERN_ASSERT(data != NULL);

	if (cons->inbuf_count == 0)
		return -1;

	int rptr = cons->inbuf_rptr;
	*data = cons->inbuf[rptr];
	cons->inbuf_rptr = (rptr + 1) % CONS_INBUF_SIZE;
	cons->inbuf_count--;

	return 0;
}

static int
cons_init_cons(struct cons *cons)
{
	KERN_ASSERT(cons != NULL);

	spinlock_init(&cons->cons_lk);

	cons->ncols = CONS_COLS;
	cons->nrows = CONS_ROWS;
	cons->cur_pos = 0;

	memzero(cons->outbuf, sizeof(uint16_t) * CONS_OUTBUF_SIZE);
	cons->outbuf_pos = 0;

	memzero(cons->inbuf, sizeof(uint32_t) * CONS_INBUF_SIZE);
	cons->inbuf_wptr = cons->inbuf_rptr = cons->inbuf_count = 0;

	return 0;
}

int
cons_init(void)
{
	KERN_ASSERT(cons_inited == FALSE);

	int i;

	serial_init();
	video_init();
	kbd_init();

	spinlock_init(&cons_switch_lk);
	spinlock_init(&cons_serial_lk);

	for (i = 0; i < MAX_CONS; i++)
		if (cons_init_cons(&consoles[i]))
			return -1;
	fg_cons = 0;

	cons_inited = TRUE;
	return 0;
}

static int
cons_putc_helper(int cons_id, char c)
{
	uint16_t raw_char;
	struct cons *cons;
	int incr, i;

	cons = &consoles[cons_id];
	KERN_ASSERT(spinlock_holding(&cons->cons_lk) == TRUE);

	raw_char = video_getraw(' ');

	switch (c) {
	case '\b':	/* backspace */
		if (cons->cur_pos > 0) {
			cons->cur_pos--;
			if (cons_id == fg_cons)
				video_putc(cons->cur_pos, ' ');

			cons->outbuf_pos += (CONS_OUTBUF_SIZE - 1);
			cons->outbuf_pos %= CONS_OUTBUF_SIZE;
			cons->outbuf[cons->outbuf_pos] = raw_char;
		}
		break;
	case '\n':	/* line feed */
		cons->cur_pos += cons->ncols;
		cons->outbuf_pos += cons->ncols;
	case '\r':	/* line carage */
		incr = cons->cur_pos % cons->ncols;
		cons->cur_pos -= incr;
		cons->outbuf_pos -= incr;
		cons->outbuf_pos %= CONS_OUTBUF_SIZE;
		break;
	case '\t':	/* table */
		for (i = 0; i < CONS_TAB_OFFSET; i++)
			cons_putc_helper(cons_id, ' ');
		break;
	default:
		if (cons_id == fg_cons)
			video_putc(cons->cur_pos, c);
		cons->cur_pos++;

		raw_char = video_getraw(c);
		cons->outbuf[cons->outbuf_pos] = raw_char;
		cons->outbuf_pos++;
		cons->outbuf_pos %= CONS_OUTBUF_SIZE;
		break;
	}

	if (cons->cur_pos >= CONS_OUTBUF_SIZE) {
		if (cons_id == fg_cons)
			video_scroll_up(1);
		cons->cur_pos -= cons->ncols;

		i = cons->outbuf_pos;
		raw_char = video_getraw(' ');
		do {
			cons->outbuf[i] = raw_char;
			i++;
		} while (i % cons->ncols);
	}

	if (cons_id == fg_cons)
		video_set_cursor(cons->cur_pos);

	return 0;
}

int
cons_putc(int cons_id, char c)
{
#if SERIAL_DEBUG
	cons_putc_serial(c);
#endif

	if (cons_inited == FALSE)
		return -1;

	KERN_ASSERT(0 <= cons_id && cons_id < MAX_CONS);

	struct cons *cons = &consoles[cons_id];
	spinlock_acquire(&cons->cons_lk);
	cons_putc_helper(cons_id, c);
	spinlock_release(&cons->cons_lk);

	return 0;
}

void
cons_putc_serial(char c)
{
	spinlock_acquire(&cons_serial_lk);
	serial_putc(c);
	spinlock_release(&cons_serial_lk);
}

int
cons_getchar(int cons_id, int *ret)
{
	if (cons_inited == FALSE)
		return -1;

	KERN_ASSERT(0 <= cons_id && cons_id < MAX_CONS);

	struct cons *cons = &consoles[cons_id];
	uint16_t data;
	uint8_t keycode, kbs;
	int c;

	spinlock_acquire(&cons->cons_lk);

	if (cons_inbuf_dequeue(cons, &data)) {
		spinlock_release(&cons->cons_lk);
		return -1;
	}

	keycode = CONS_GET_KEYCODE(data);
	kbs = CONS_GET_KDBSTAT(data);

	/* ignore key release */
	if ((kbs & KBS_RELEASE) || keycode == KEY_NOP) {
		spinlock_release(&cons->cons_lk);
		return -1;
	}

	c = ((kbs & (KBS_SHIFT | KBS_CTRL | KBS_ALT)) == KBS_SHIFT) ?
		shiftmap[keycode] : normalmap[keycode];

	/* ignore non-ascii keys */
	if (c == 0) {
		spinlock_release(&cons->cons_lk);
		return -1;
	}

	if (kbs & KBS_CAPSLOCK) {
		if ('a' <= c && c <= 'z')
			c += 'A' - 'a';
		else if ('A' <= c && c <= 'Z')
			c += 'a' - 'A';
	}

	*ret = c;

	cons_putc_helper(cons_id, (char) c);

	spinlock_release(&cons->cons_lk);

	return 0;
}

int
cons_getraw(int cons_id, uint16_t *raw)
{
	if (cons_inited == FALSE)
		return -1;

	KERN_ASSERT(0 <= cons_id && cons_id < MAX_CONS);

	struct cons *cons = &consoles[cons_id];
	uint16_t data;

	spinlock_acquire(&cons->cons_lk);

	if (cons_inbuf_dequeue(cons, &data)) {
		spinlock_release(&cons->cons_lk);
		return -1;
	}

	*raw = data;

	spinlock_release(&cons->cons_lk);

	return 0;
}

static void
cons_flush_outbuf(struct cons *cons)
{
	KERN_ASSERT(cons != NULL);
	KERN_ASSERT(spinlock_holding(&cons->cons_lk) == TRUE);

	int outbuf_pos, cur_pos;
	int scroll_back;

	outbuf_pos = cons->outbuf_pos;
	cur_pos = cons->cur_pos;
	scroll_back = cur_pos - outbuf_pos;

	if (scroll_back > 0) {
		video_buf_write(0,
				&cons->outbuf[CONS_OUTBUF_SIZE - scroll_back],
				scroll_back);
		video_buf_write(scroll_back,
				cons->outbuf, outbuf_pos);
	} else {
		video_buf_write(0,
				&cons->outbuf[-scroll_back],
				cur_pos);
	}
	if (cur_pos < CONS_OUTBUF_SIZE - 1) {
		video_buf_clear(cur_pos, CONS_OUTBUF_SIZE - cur_pos);
	}

	video_set_cursor(cur_pos);
}

int
cons_switch(int to_cons_id)
{
	if (cons_inited == FALSE)
		return -1;

	KERN_ASSERT(0 <= to_cons_id && to_cons_id < MAX_CONS);

	struct cons *from_cons, *to_cons;

	spinlock_acquire(&cons_switch_lk);

	from_cons = (fg_cons == -1) ? NULL : &consoles[fg_cons];
	to_cons = &consoles[to_cons_id];

	if (from_cons == to_cons) {
		spinlock_release(&cons_switch_lk);
		return 0;
	}

	if (from_cons != NULL)
		spinlock_acquire(&from_cons->cons_lk);
	spinlock_acquire(&to_cons->cons_lk);

	cons_flush_outbuf(to_cons);
	fg_cons = to_cons_id;

	spinlock_release(&to_cons->cons_lk);
	if (from_cons != NULL)
		spinlock_release(&from_cons->cons_lk);

	spinlock_release(&cons_switch_lk);
	return 0;
}

int
cons_kbd_intr_handler(uint8_t trapno, struct context *ctx)
{
	KERN_ASSERT(cons_inited == TRUE);
	KERN_ASSERT(trapno == T_IRQ0 + IRQ_KBD);

	struct cons *cons;

	spinlock_acquire(&cons_switch_lk);

	if (fg_cons == -1) {
		intr_eoi();
		spinlock_release(&cons_switch_lk);
		return 0;
	}

	cons = &consoles[fg_cons];

	spinlock_acquire(&cons->cons_lk);

	kbd_fill_inbuf(cons_inbuf_enqueue_wrapper, cons);
	intr_eoi();

	spinlock_release(&cons->cons_lk);

	spinlock_release(&cons_switch_lk);

	return 0;
}
