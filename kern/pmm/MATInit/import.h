#ifndef _KERN_MM_MALINIT_H_
#define _KERN_MM_MALINIT_H_

#ifdef _KERN_

void set_nps(unsigned int nps);
void set_norm(unsigned int idx, unsigned int val);
unsigned int devinit(unsigned int);
unsigned int get_size(void);
unsigned int get_mms(unsigned int);
unsigned int get_mml(unsigned int);
unsigned int is_usable(unsigned int);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MALINIT_H_ */
