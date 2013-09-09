#ifndef _KERN_MM_MALINIT_H_
#define _KERN_MM_MALINIT_H_

#ifdef _KERN_

int  get_nps();
void set_nps(int nps);

int  is_norm(int idx);
void set_norm(int idx, int val);

int  at_get(int idx);
void at_set(int idx, int val);

/*
 * Derived from lower layers.
 */

void set_pe(void);
void set_pt(int *);
int  pmmap_init(unsigned int mbi_addr);
int  pmmap_entries_nr(void);
int  pmmap_entry_start(int idx);
int  pmmap_entry_length(int idx);
int  pmmap_entry_usable(int idx);
int  is_usable(int);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MALINIT_H_ */
