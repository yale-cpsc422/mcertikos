#include <sys/console.h>
#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <machine/trap.h>

#include <dev/kbd.h>
#include <dev/ioapic.h>
#include <dev/lapic.h>
#include <dev/pic.h>

#include <sys/vm.h>
#include <sys/virt/vmm.h>


#define NO		0

#define SHIFT		(1<<0)
#define CTL		(1<<1)
#define ALT		(1<<2)
#define CAPSLOCK	(1<<3)
#define NUMLOCK		(1<<4)
#define SCROLLLOCK	(1<<5)
#define E0ESC		(1<<6)


static uint8_t shiftcode[256] =
{
	[0x1D] = CTL,
	[0x2A] = SHIFT,
	[0x36] = SHIFT,
	[0x38] = ALT,
	[0x9D] = CTL,
	[0xB8] = ALT
};

static uint8_t togglecode[256] =
{
	[0x3A] = CAPSLOCK,
	[0x45] = NUMLOCK,
	[0x46] = SCROLLLOCK
};

static uint8_t normalmap[256] =
{
	NO,   0x1B, '1',  '2',  '3',  '4',  '5',  '6',	// 0x00
	'7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
	'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',	// 0x10
	'o',  'p',  '[',  ']',  '\n', NO,   'a',  's',
	'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',	// 0x20
	'\'', '`',  NO,   '\\', 'z',  'x',  'c',  'v',
	'b',  'n',  'm',  ',',  '.',  '/',  NO,   '*',	// 0x30
	NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
	NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',	// 0x40
	'8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
	'2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,	// 0x50
	[0xC7] = KEY_HOME,	[0x9C] = '\n' /*KP_Enter*/,
	[0xB5] = '/' /*KP_Div*/,[0xC8] = KEY_UP,
	[0xC9] = KEY_PGUP,	[0xCB] = KEY_LF,
	[0xCD] = KEY_RT,	[0xCF] = KEY_END,
	[0xD0] = KEY_DN,	[0xD1] = KEY_PGDN,
	[0xD2] = KEY_INS,	[0xD3] = KEY_DEL
};

static uint8_t shiftmap[256] =
{
	NO,   033,  '!',  '@',  '#',  '$',  '%',  '^',	// 0x00
	'&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
	'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',	// 0x10
	'O',  'P',  '{',  '}',  '\n', NO,   'A',  'S',
	'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',	// 0x20
	'"',  '~',  NO,   '|',  'Z',  'X',  'C',  'V',
	'B',  'N',  'M',  '<',  '>',  '?',  NO,   '*',	// 0x30
	NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
	NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',	// 0x40
	'8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
	'2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,	// 0x50
	[0xC7] = KEY_HOME,	[0x9C] = '\n' /*KP_Enter*/,
	[0xB5] = '/' /*KP_Div*/,[0xC8] = KEY_UP,
	[0xC9] = KEY_PGUP,	[0xCB] = KEY_LF,
	[0xCD] = KEY_RT,	[0xCF] = KEY_END,
	[0xD0] = KEY_DN,	[0xD1] = KEY_PGDN,
	[0xD2] = KEY_INS,	[0xD3] = KEY_DEL
};

#define C(x) (x - '@')

static uint8_t ctlmap[256] =
{
	NO,      NO,      NO,      NO,      NO,      NO,      NO,      NO,
	NO,      NO,      NO,      NO,      NO,      NO,      NO,      NO,
	C('Q'),  C('W'),  C('E'),  C('R'),  C('T'),  C('Y'),  C('U'),  C('I'),
	C('O'),  C('P'),  NO,      NO,      '\r',    NO,      C('A'),  C('S'),
	C('D'),  C('F'),  C('G'),  C('H'),  C('J'),  C('K'),  C('L'),  NO,
	NO,      NO,      NO,      C('\\'), C('Z'),  C('X'),  C('C'),  C('V'),
	C('B'),  C('N'),  C('M'),  NO,      NO,      C('/'),  NO,      NO,
	[0x97] = KEY_HOME,
	[0xB5] = C('/'),	[0xC8] = KEY_UP,
	[0xC9] = KEY_PGUP,	[0xCB] = KEY_LF,
	[0xCD] = KEY_RT,	[0xCF] = KEY_END,
	[0xD0] = KEY_DN,	[0xD1] = KEY_PGDN,
	[0xD2] = KEY_INS,	[0xD3] = KEY_DEL
};

static uint8_t *charcode[4] = {
	normalmap,
	shiftmap,
	ctlmap,
	ctlmap
};

/*
 * Get data from the keyboard.  If we finish a character, return it.  Else 0.
 * Return -1 if no data.
 */
static int
kbd_proc_data(void)
{
	int c;
	uint8_t data;
	static uint32_t shift;

	struct vm *vm = vmm_cur_vm();
	bool from_guest =
		(vm != NULL && vm->exit_for_intr == TRUE) ? TRUE : FALSE;

	if ((inb(KBSTATP) & KBS_DIB) == 0)
		return -1;

	data = inb(KBDATAP);

	if (from_guest == TRUE) {
		inject_vkbd_queue(&vm->vkbd, data, 0);
		return 0;
	}

	if (data == 0xE0) {
		// E0 escape character
		shift |= E0ESC;
		return 0;
	} else if (data & 0x80) {
		// Key released
		data = (shift & E0ESC ? data : data & 0x7F);
		shift &= ~(shiftcode[data] | E0ESC);
		return 0;
	} else if (shift & E0ESC) {
		// Last character was an E0 escape; or with 0x80
		data |= 0x80;
		shift &= ~E0ESC;
	}

	shift |= shiftcode[data];
	shift ^= togglecode[data];

	c = charcode[shift & (CTL | SHIFT)][data];
	if (shift & CAPSLOCK) {
		if ('a' <= c && c <= 'z')
			c += 'A' - 'a';
		else if ('A' <= c && c <= 'Z')
			c += 'a' - 'A';
	}

	// Process special keys
#if LAB >= 99
#if CRT_SAVEROWS > 0
	// Shift-PageUp and Shift-PageDown: scroll console
	if ((shift & SHIFT) && (c == KEY_PGUP || c == KEY_PGDN)) {
		cga_scroll(c == KEY_PGUP ? -CRT_ROWS : CRT_ROWS);
		return 0;
	}
#endif
#endif
	// Ctrl-Alt-Del: reboot
	if (!(~shift & (CTL | ALT)) && c == KEY_DEL) {
		cprintf("Rebooting!\n");
		outb(0x92, 0x3); // courtesy of Chris Frost
	}

	return c;
}

void
kbd_intr(void)
{
	cons_intr(kbd_proc_data);
}

static void
kbd_wait4_input_empty(void)
{
	uint8_t status;

	/* KERN_DEBUG("Waiting for ~IBF...\n"); */

	while (1) {
		status = inb(KBSTATP);
		if (!(status & KBS_IBF))
			break;
	}
}

static void
kbd_wait4_output_full(void)
{
	uint8_t status;

	/* KERN_DEBUG("Waiting for OBF...\n"); */

	while (1) {
		status = inb(KBSTATP);
		if (status & KBS_DIB)
			break;
	}
}

static void
kbd_disable(void)
{
	kbd_wait4_input_empty();
	outb(KBCMDP, KBC_KBDDISABLE);
}

static void
kbd_send_cmd(uint8_t cmd)
{
	kbd_wait4_input_empty();
	outb(KBCMDP, KBC_RAMWRITE);
	kbd_wait4_input_empty();
	/* KERN_DEBUG("Send command %x.\n", cmd); */
	outb(KBDATAP, cmd);
}

void
kbd_init(void)
{
/* 	uint8_t status; */

/* 	/\* self-test *\/ */
/* 	kbd_wait4_input_empty(); */
/* 	outb(KBCMDP, KBC_SELFTEST); */
/* 	kbd_wait4_output_full(); */
/* 	status = inb(KBDATAP); */
/* 	if (status != 0x55) { */
/* 		KERN_DEBUG("i8042 self-test failed.\n"); */
/* 		goto fail; */
/* 	} */
/* 	/\* KERN_DEBUG("i8042 self-test is ok.\n"); *\/ */

/* 	/\* keyboard interface test *\/ */
/* 	kbd_wait4_input_empty(); */
/* 	outb(KBCMDP, KBC_KBDTEST); */
/* 	kbd_wait4_output_full(); */
/* 	status = inb(KBDATAP); */
/* 	if (status != 0x00) { */
/* 		KERN_DEBUG("Keyboard interface test failed, code=%x.\n", status); */
/* 		goto fail; */
/* 	} */
/* 	/\* KERN_DEBUG("Keyboard interface test is ok.\n"); *\/ */

/* 	kbd_send_cmd(0x30); */
/* 	kbd_send_cmd(0x20); */

/* 	/\* reset *\/ */
/* 	kbd_wait4_input_empty(); */
/* 	outb(KBDATAP, KBC_RESET); */
/* 	kbd_wait4_output_full(); */
/* 	status = inb(KBDATAP); */
/* 	if (status != KBR_ACK) { */
/* 		KERN_DEBUG("Keyborad reset failed.\n"); */
/* 		goto fail; */
/* 	} */
/* 	kbd_wait4_output_full(); */
/* 	status = inb(KBDATAP); */
/* 	if (status != KBR_RSTDONE) { */
/* 		KERN_DEBUG("Keyborad reset failed.\n"); */
/* 		goto fail; */
/* 	} */
/* 	/\* KERN_DEBUG("Keyboard reset is ok.\n"); *\/ */

/* 	/\* */
/* 	 * FIXME: This piece of code does work in Simnow, but works well in QEMU */
/* 	 *        and BOCHS. */
/* 	 *\/ */
/* #if 0 */
/* 	kbd_send_cmd(0x30); */
/* 	kbd_send_cmd(0x20); */

/* 	/\* reset keyboard to power-on condition *\/ */
/* 	kbd_wait4_input_empty(); */
/* 	outb(KBDATAP, KBC_DISABLE); */
/* 	kbd_wait4_output_full(); */
/* 	status = inb(KBDATAP); */
/* 	if (status != KBR_ACK) { */
/* 		KERN_DEBUG("Reset keyboard failed.\n"); */
/* 		goto fail; */
/* 	} */
/* 	/\* KERN_DEBUG("Reset keyboard is ok.\n"); *\/ */
/* #endif */

/* 	kbd_send_cmd(0x30); */
/* 	kbd_send_cmd(0x20); */

/* 	/\* select to scancode set 2 *\/ */
/* 	kbd_wait4_input_empty(); */
/* 	outb(KBDATAP, KBC_SETTABLE); */
/* 	kbd_wait4_output_full(); */
/* 	status = inb(KBDATAP); */
/* 	if (status != KBR_ACK) { */
/* 		KERN_DEBUG("Select scancode set failed.\n"); */
/* 		goto fail; */
/* 	} */
/* 	kbd_wait4_input_empty(); */
/* 	outb(KBDATAP, 0x2); */
/* 	kbd_wait4_output_full(); */
/* 	status = inb(KBDATAP); */
/* 	if (status != KBR_ACK) { */
/* 		KERN_DEBUG("Select scancode set failed.\n"); */
/* 		goto fail; */
/* 	} */
/* 	/\* KERN_DEBUG("Select scanmode is ok.\n"); *\/ */

/* 	/\* set to PC/XT mode *\/ */
/* 	kbd_send_cmd(0x30); */
/* 	kbd_send_cmd(0x70); */
/* 	kbd_send_cmd(0x60); */

/* 	/\* enable KBD *\/ */
/* 	kbd_wait4_input_empty(); */
/* 	outb(KBDATAP, KBC_ENABLE); */
/* 	kbd_wait4_output_full(); */
/* 	status = inb(KBDATAP); */
/* 	if (status != KBR_ACK) { */
/* 		KERN_DEBUG("Enable keyboard failed.\n"); */
/* 		goto fail; */
/* 	} */
/* 	/\* KERN_DEBUG("Enable keyboard is ok.\n"); *\/ */

/* 	kbd_send_cmd(0x61); */

/* 	return; */

/*  fail: */
/* 	KERN_DEBUG("status=%x.\n", status); */
/* 	KERN_DEBUG("Disable keyboard.\n"); */
/* 	kbd_disable(); */
/* 	return; */
}

void
kbd_intenable(void)
{
	// Enable interrupt delivery via the PIC/APIC
	intr_enable(IRQ_KBD, 0);

	// Drain the kbd buffer so that the hardware generates interrupts.
	kbd_intr();
}
