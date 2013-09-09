#ifndef _KERN_MM_MPTINTRO_H_
#define _KERN_MM_MPTINTRO_H_

#ifdef _KERN_

void set_PDX(int proc_idx, int pdx);
unsigned int get_PTX(int proc_index, int pdx, int ptx);
void set_PTX(int proc_idx, int pdx, int ptx, unsigned int pa, int perm);
void rmv_PTX(int proc_index, int pdx, int ptx);
void pt_in(void);
void pt_out(void);
void set_PT(int idx);
void set_PTX_P(int proc_index, int pdx, int ptx);

/*
 * Derived from lower layers.
 */

void pfree(int idx);
int  palloc(void);
void mem_init(unsigned int mbi_addr);
void set_pe(void);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MPTINTRO_H_ */
