#ifndef _KERN_MM_MALINIT_H_
#define _KERN_MM_MALINIT_H_

unsigned int get_nps(void);
void set_nps(unsigned int page_index);

unsigned int at_is_norm(unsigned int page_index);
void at_set_perm(unsigned int page_index, unsigned int norm_val);

unsigned int at_is_allocated(unsigned int page_index);
void at_set_allocated(unsigned int page_index, unsigned int allocated);

#endif
