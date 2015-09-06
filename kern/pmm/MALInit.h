#ifndef _KERN_MM_MALINIT_H_
#define _KERN_MM_MALINIT_H_

#ifdef _KERN_

/*
 * Primitives defined by this layer.
 */

unsigned int get_nps(void);
void set_nps(unsigned int nps);

unsigned int is_norm(unsigned int idx);
void set_norm(unsigned int idx, unsigned int val);

unsigned int at_get(unsigned int idx);
void at_set(unsigned int idx, unsigned int val);

unsigned int at_get_c(unsigned int idx);
void at_set_c(unsigned int idx, unsigned int val);

/*
 * Primitives derived from the lower layer.
 */

void set_pg(void);
void set_cr3(char **);
unsigned int devinit(unsigned int);
unsigned int get_size(void);
unsigned int get_mms(unsigned int);
unsigned int get_mml(unsigned int);
unsigned int is_usable(unsigned int);


#endif /* _KERN_ */

#endif /* !_KERN_MM_MALINIT_H_ */
