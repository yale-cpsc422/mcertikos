#ifndef _KERN_MM_MALOP_H_
#define _KERN_MM_MALOP_H_

#ifdef _KERN_

unsigned int get_nps(void);
unsigned int is_norm(unsigned int idx);
unsigned int at_get(unsigned int idx);
void at_set(unsigned int idx, unsigned int val);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MALOP_H_ */
