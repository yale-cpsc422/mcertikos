#ifndef _KERN_MM_MPTINTRO_H_
#define _KERN_MM_MPTINTRO_H_

#ifdef _KERN_

void set_PDX(unsigned int pid, unsigned int pdx);
unsigned int get_PTX(unsigned int proc_index, unsigned int pdx, unsigned int ptx);
void set_PTX(unsigned int pid, unsigned int pdx, unsigned int ptx,
	     unsigned int pa, unsigned int perm);
void rmv_PTX(unsigned int pid, unsigned int pdx, unsigned int ptx);
void set_PT(unsigned int idx);
void set_PTX_P(unsigned int pid, unsigned int pdx, unsigned int ptx);

void pt_in(void);
void pt_out(void);

/*
 * Derived from lower layers.
 */

void pfree(unsigned int idx);
unsigned int palloc(void);
void mem_init(unsigned int mbi_addr);
void set_pe(void);


#endif /* _KERN_ */

#endif /* !_KERN_MM_MPTINTRO_H_ */
