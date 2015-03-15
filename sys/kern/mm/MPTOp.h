#ifndef _KERN_MM_MPTOP_H_
#define _KERM_MM_MPTOP_H_

#ifdef _KERN_

/*
 * Primitives defined by this layer.
 */

unsigned int pt_read(unsigned int, unsigned int);
unsigned int pt_read_pde(unsigned int, unsigned int);
void pt_rmv_aux(unsigned int, unsigned int);
void pt_rmv_pde(unsigned int, unsigned int);
void pt_insert_aux(unsigned int, unsigned int, unsigned int, unsigned int);
void pt_insert_pde(unsigned int, unsigned int, unsigned int);
void idpde_init(unsigned int);

/*
 * Primitives derived from lower layers.
 */

void set_PDE(unsigned int pid, unsigned int pdx);
void rmv_PDE(unsigned int, unsigned int);
void rmv_PTE(unsigned int, unsigned int, unsigned int);
void pt_in(void);
void pt_out(void);
void pfree(unsigned int idx);
unsigned int palloc(void);
void mem_init(unsigned int mbi_addr);
void set_pg(void);
void set_pt(unsigned int idx);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MPTOP_H_ */
