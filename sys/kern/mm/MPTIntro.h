#ifndef _KERN_MM_MPTINTRO_H_
#define _KERN_MM_MPTINTRO_H_

#ifdef _KERN_

void set_pt(unsigned int);
unsigned int get_PDE(unsigned int, unsigned int);
void set_PDE(unsigned int, unsigned int);
void rmv_PDE(unsigned int, unsigned int);
void set_PDEU(unsigned int, unsigned int, unsigned int);
unsigned int get_PTE(unsigned int, unsigned int, unsigned int);
void set_PTE(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
void rmv_PTE(unsigned int, unsigned int, unsigned int);
void set_IDPTE(unsigned int, unsigned int, unsigned int);

void pt_in(void);
void pt_out(void);

/*
 * Derived from lower layers.
 */

void pfree(unsigned int idx);
unsigned int palloc(void);
void mem_init(unsigned int mbi_addr);
void set_pg(void);


#endif /* _KERN_ */

#endif /* !_KERN_MM_MPTINTRO_H_ */
