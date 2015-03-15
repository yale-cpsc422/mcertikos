#include "MPTIntro.h"

#define PAGESIZE	4096
#define PT_PERM_PTKF 3
#define PT_PERM_PTKT 259


unsigned int
pt_read(unsigned int proc_index, unsigned int vaddr)
{
    unsigned int pdx_index;
    unsigned int vaddrl;
    unsigned int paddr;
    pdx_index = vaddr / (4096 * 1024);
    vaddrl = (vaddr / 4096) % 1024;
    paddr = get_PTE(proc_index, pdx_index, vaddrl);
    return paddr;
}         

unsigned int
pt_read_pde(unsigned int proc_index, unsigned int vaddr)
{
    unsigned int pdx_index;
    unsigned int paddr;
    pdx_index = vaddr / (4096 * 1024);
    paddr = get_PDE(proc_index, pdx_index);
    return paddr;
}

void
pt_rmv_aux(unsigned int proc_index, unsigned int vaddr)
{
    unsigned int pdx_index;
    unsigned int vaddrl;
    pdx_index = vaddr / (4096 * 1024);
    vaddrl = (vaddr / 4096) % 1024;
    rmv_PTE(proc_index, pdx_index, vaddrl);
}

void
pt_rmv_pde(unsigned int proc_index, unsigned int vaddr)
{
    unsigned int pdx_index;
    pdx_index = vaddr / (4096 * 1024);
    rmv_PDE(proc_index, pdx_index);
}

void
pt_insert_aux(unsigned int proc_index, unsigned int vaddr, unsigned int paddr, unsigned int perm)
{
    unsigned int pdx_index;
    unsigned int vaddrl;
    pdx_index = vaddr / (4096 * 1024);
    vaddrl = (vaddr / 4096) % 1024;
    set_PTE(proc_index, pdx_index, vaddrl, paddr, perm);
}

void
pt_insert_pde(unsigned int proc_index, unsigned int vaddr, unsigned int pi)
{
    unsigned int pdx_index;
    pdx_index = vaddr / (4096 * 1024);
    set_PDEU(proc_index, pdx_index, pi);
}   

void
idpde_init(unsigned int mbi_adr)
{
  unsigned int i, j;
  unsigned int perm;
  mem_init(mbi_adr);
  i = 0;
  while(i < 1024)
  {
    if (i < 256)
      perm = PT_PERM_PTKT;
    else if (i >= 960)
      perm = PT_PERM_PTKT;
    else
      perm = PT_PERM_PTKF;
    j = 0;
    while(j < 1024)
    {
      set_IDPTE(i, j, perm);
      j ++;
    }
    i ++;
  }
}
