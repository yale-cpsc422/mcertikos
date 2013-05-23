#include <sys/debug.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <dev/cons_kbd.h>
#include <dev/kbd.h>

/* i8042 ports */
#define KBSTATP		0x64
# define KBS_DIB	0x01
#define KBCMDP		0x64
#define KBDATAP		0x60
#define KBOUTP		0x60

/* shifts */
#define E0ESC		(1 << 0)
#define SHIFT		(1 << 1)
#define ALT		(1 << 2)
#define CTRL		(1 << 3)
#define WIN		(1 << 4)
#define CAPSLOCK	(1 << 5)
#define SCROLLLOCK	(1 << 6)
#define NUMLOCK		(1 << 7)

static volatile int shift;

static const uint8_t normalmap[256] = {
	KEY_NOP,					/* 0x00 */
	KEY_ESC,					/* 0x01 */
	KEY_1, KEY_2, KEY_3, KEY_4, KEY_5,		/* 0x02 ~ 0x06 */
	KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,		/* 0x07 ~ 0x0b */
	KEY_MINUS, KEY_PLUS, KEY_BACKSPACE,		/* 0x0c ~ 0x0e */
	KEY_TAB,					/* 0x0f */
	KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y,	/* 0x10 ~ 0x15 */
	KEY_U, KEY_I, KEY_O, KEY_P,			/* 0x16 ~ 0x19 */
	KEY_L_BRACK, KEY_R_BRACK,			/* 0x1a ~ 0x1b */
	KEY_ENTER,					/* 0x1c */
	KEY_L_CTRL,					/* 0x1d */
	KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H,	/* 0x1e ~ 0x23 */
	KEY_J, KEY_K, KEY_L,				/* 0x24 ~ 0x26 */
	KEY_SEMICOLON, KEY_QUOTE,			/* 0x27 ~ 0x28 */
	KEY_TIDE,					/* 0x29 */
	KEY_L_SHIFT,					/* 0x2a */
	KEY_BACKSLASH,					/* 0x2b */
	KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M,/* 0x2c ~ 0x32 */
	KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_R_SHIFT,	/* 0x33 ~ 0x36 */
	KEY_KP_MULT,					/* 0x37 */
	KEY_L_ALT, KEY_SPACE, KEY_CAPSLOCK,		/* 0x38 ~ 0x3a */
	KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,	/* 0x3b ~ 0x40 */
	KEY_F7, KEY_F8, KEY_F9, KEY_F10,		/* 0x41 ~ 0x44 */
	KEY_NUMLOCK, KEY_SCROLLLOCK,			/* 0x45 ~ 0x46 */
	KEY_KP_7, KEY_KP_8, KEY_KP_9,			/* 0x47 ~ 0x49 */
	KEY_KP_MINUS,					/* 0x4a */
	KEY_KP_4, KEY_KP_5, KEY_KP_6, KEY_KP_PLUS,	/* 0x4b ~ 0x4e */
	KEY_KP_1, KEY_KP_2, KEY_KP_3,			/* 0x4f ~ 0x51 */
	KEY_KP_0, KEY_KP_DOT,				/* 0x52 ~ 0x53 */
	[0x57] = KEY_F11, [0x58] = KEY_F12,
};

static const uint8_t escapedmap[256] = {
	[0x1c] = KEY_KP_ENTER,
	[0x1d] = KEY_R_CTRL,
	[0x2a] = KEY_NOP,
	[0x35] = KEY_KP_DIV,
	[0x36] = KEY_NOP,
	[0x37] = KEY_NOP,
	[0x38] = KEY_R_ALT,
	[0x46] = KEY_NOP,
	[0x47] = KEY_HOME,
	[0x48] = KEY_UP,
	[0x49] = KEY_PAGEUP,
	[0x4b] = KEY_LEFT,
	[0x4d] = KEY_RIGHT,
	[0x4f] = KEY_END,
	[0x50] = KEY_DOWN,
	[0x51] = KEY_PAGEDOWN,
	[0x52] = KEY_INSERT,
	[0x53] = KEY_DELETE,
	[0x5b] = KEY_L_WIN,
	[0x3c] = KEY_R_WIN,
	[0x5d] = KEY_MENU,
};

static const uint8_t shiftcode[256] = {
	[0x1d] = CTRL,
	[0x2a] = SHIFT,
	[0x36] = SHIFT,
	[0x38] = ALT,
};

static const uint8_t togglecode[256] =
{
	[0x3a] = CAPSLOCK,
	[0x45] = NUMLOCK,
	[0x46] = SCROLLLOCK,
};

void
kbd_init(void)
{
	shift = 0;
}

static int
kbd_read_scancode(uint8_t *scancode)
{
	if ((inb(KBSTATP) & KBS_DIB) == 0)
		return -1;

	*scancode = inb(KBDATAP);

	return 0;
}

int
kbd_fill_inbuf(int (*fill)(void *, uint16_t), void *param)
{
	uint8_t scancode, data, keycode, kbs;

	while (kbd_read_scancode(&scancode) != -1) {
		/* KERN_DEBUG("Scancode %02xh\n", scancode); */

		if (scancode == 0xE0) {
			shift |= E0ESC;
			continue;
		}

		if (scancode & 0x80) { /* key released */
			data = scancode & 0x7f;
			/* ignore fake shift */
			if ((shift & E0ESC) && shiftcode[data] == SHIFT) {
				shift &= ~E0ESC;
				continue;
			}
		} else if (shift & E0ESC) { /* key pressed and after escape */
			data = scancode;
			/* ignore fake shift */
			if (shiftcode[data] == SHIFT) {
				shift &= ~E0ESC;
				continue;
			}
		} else {
			data = scancode;
		}

		if (scancode & 0x80) {
			shift &= ~shiftcode[data];
		} else {
			shift |= shiftcode[data];
			shift ^= togglecode[data];
		}

		keycode = (shift & E0ESC) ? escapedmap[data] : normalmap[data];
		kbs = (shift & 0xfe) | ((scancode & 0x80) ? KBS_RELEASE : 0x0);
		fill(param, CONS_MAKE_KBDINPUT(kbs, keycode));

		/* KERN_DEBUG("Fill (%02xh, %02xh)\n", kbs, keycode); */

		shift &= ~E0ESC;
	}

	return 0;
}
