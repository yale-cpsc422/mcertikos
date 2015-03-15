#ifndef _KERN_MM_MBOOT_H_
#define _KERN_MM_MBOOT_H_

#ifdef _KERN_

/*
 * Primitives defined by this layer.
 */

unsigned int fload(unsigned int);
void fstore(unsigned int, unsigned int);

/*
 * Primitives derived from the lower layer.
 */

void set_pg(void);
void set_cr3(char **);
unsigned int preinit(unsigned int);
unsigned int get_size(void);
unsigned int get_mms(unsigned int);
unsigned int get_mml(unsigned int);
unsigned int is_usable(unsigned int);


#endif /* _KERN_ */

#endif /* !_KERN_MM_MBOOT_H_ */
