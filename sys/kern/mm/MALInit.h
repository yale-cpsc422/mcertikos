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

/*
 * Primitives derived from the lower layer.
 */

void set_pe(void);
void set_pt(unsigned int *);
unsigned int pmmap_init(unsigned int mbi_addr);
unsigned int pmmap_entries_nr(void);
unsigned int pmmap_entry_start(unsigned int idx);
unsigned int pmmap_entry_length(unsigned int idx);
unsigned int pmmap_entry_usable(unsigned int idx);
unsigned int is_usable(unsigned int);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MALINIT_H_ */
