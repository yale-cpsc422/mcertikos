#ifndef _KERN_MM_MAL_OP_H_
#define _KERN_MM_MAL_OP_H_

#ifdef _KERN_

int palloc(void);
void pfree(int idx);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MAL_OP_H_ */
