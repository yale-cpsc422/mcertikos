#ifndef _KERN_MM_MALOP_H_
#define _KERN_MM_MALOP_H_

#ifdef _KERN_

void mem_init(unsigned int mbi_addr);

/*
 * Derived from lower layers.
 */

int  get_nps(void);
int  is_norm(int idx);
int  at_get(int idx);
void at_set(int idx, int val);
void set_pe(void);
void set_pt(int *);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MALOP_H_ */
