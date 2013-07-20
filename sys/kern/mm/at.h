#ifndef _KERN_MM_AT_H_
#define _KERN_MM_AT_H_

#ifdef _KERN_

int at_get_nps(void);
void at_set_nps(int val);
int at_is_norm(int idx);
void at_set_norm(int idx, int val);
int at_get(int idx);
void at_set(int idx, int val);

#endif /* _KERN_ */

#endif /* !_KERN_MM_AT_H_ */
