#ifndef _KERN_DEV_KBD_H_
#define _KERN_DEV_KBD_H_

#ifdef _KERN_

void kbd_init(void);

int kbd_fill_inbuf(int (*fill)(void *, uint16_t), void *param);

#endif /* _KERN_ */

#endif /* !_KERN_DEV_KBD_H_ */
