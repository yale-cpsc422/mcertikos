#ifndef _KERN_MM_MPTCOMM_H_
#define _KERN_MM_MPTCOMM_H_

#ifdef _KERN_

void pt_init_comm(unsigned int mbi_addr);
unsigned int pt_alloc_pde(unsigned int, unsigned int);
void pt_free_pde(unsigned int, unsigned int);

/*
 * Derived from lower layers.
 */

void set_PDE(unsigned int pid, unsigned int pdx);
unsigned int pt_read(unsigned int, unsigned int);
unsigned int pt_read_pde(unsigned int, unsigned int);
void pt_insert_aux(unsigned int, unsigned int, unsigned int, unsigned int);
void pt_rmv_aux(unsigned int, unsigned int);
unsigned int at_get_c(unsigned int idx);
void at_set_c(unsigned int idx, unsigned int val);
void pt_in(void);
void pt_out(void);
void pfree(unsigned int idx);
unsigned int palloc(void);
void set_pg(void);
void set_pt(unsigned int idx);

#endif /* _KERN_ */

#endif /* !_KERN_MM_MPTCOMM_H_ */
