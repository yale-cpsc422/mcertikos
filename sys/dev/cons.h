#ifndef _KERN_DEV_CONS_H_
#define _KERN_DEV_CONS_H_

#ifdef _KERN_

#include <sys/context.h>

#define MAX_CONS		8

#define CONS_KBS_L_CTRL		0	/* left ctrl is being pressed */
#define CONS_KBS_R_CTRL		1	/* right ctrl is being pressed */
#define CONS_KBS_L_ALT		2	/* left alt is being pressed */
#define CONS_KBS_R_ALT		3	/* right alt is being pressed */
#define CONS_KBS_L_SHIFT	4	/* left shift is being pressed */
#define CONS_KBS_R_SHIFT	5	/* right shift is being pressed */
#define CONS_KBS_CAPLOCK	6	/* CapsLock is on */
#define CONS_KBS_SCRLOCK	7	/* ScrollLock is on */
#define CONS_KBS_NUMLOCK	8	/* NumLock is on */

/*
 * Initialize all consoles.
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int cons_init(void);

/*
 * Output a character to a console. If the console is in the foreground, the
 * character will be displayed on the screen as well.
 *
 * @param cons_id the identity of the console
 * @param c       the character
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int cons_putc(int cons_id, char c);

/*
 * Output a character to the serial port.
 *
 * @param c the character
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
void cons_putc_serial(char c);

/*
 * Get an ASCII character from a console.
 *
 * @param cons_id the identity of the console
 * @param c       if successful, the character will be stored at this position
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int cons_getchar(int cons_id, int *c);

/*
 * Get the 32-bit raw data from a console.
 *
 * @param cons_id the identity of the console
 * @param raw     if successful, the raw data will be stored at this position
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int cons_getraw(int cons_id, uint16_t *raw);

/*
 * The keyboard interrupt handler.
 */
int cons_kbd_intr_handler(uint8_t trapno, struct context *ctx);

/*
 * TODO: remove later
 */
int cons_switch(int cons_id);

#endif /* _KERN_ */

#endif /* !_KERN_DEV_CONS_H_ */
