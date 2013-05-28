#ifndef _KERN_CONSOLE_H_
#define _KERN_CONSOLE_H_

#ifndef _KERN_
#error "This is a kernel header file; do not include it in userspace program."
#endif /* !_KERN_ */

#define CONSOLE_BUFFER_SIZE	512

void cons_init(void);
void cons_enable_kbd(void);
void cons_putc(char);
int cons_getc();
void cons_intr(int (*proc)(void));

#endif /* !_KERN_CONSOLE_H_ */
