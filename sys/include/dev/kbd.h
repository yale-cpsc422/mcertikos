#ifndef _KERN_DEV_KBD_H_
#define _KERN_DEV_KBD_H_

#ifndef _KERN_
#error "This is a kernel header file; do not include it in userspace programs."
#endif

void kbd_init(void);
void kbd_intenable(void);
void kbd_intr(void);

#endif /* !_KERN_DEV_KBD_H_ */
