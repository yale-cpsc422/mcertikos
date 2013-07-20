#ifndef _KERN_DEV_KBD_H_
#define _KERN_DEV_KBD_H_

#ifdef _KERN_

void kbd_init(void);
void kbd_intr(void);
void kbd_intenable(void);

#endif /* _KERN_ */

#endif /* !_KERN_DEV_KBD_H_ */
