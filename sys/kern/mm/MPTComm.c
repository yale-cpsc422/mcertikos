#include "MPTOp.h"


#define num_proc 64
#define one_k 1024
#define PAGESIZE 4096

void
pt_init_comm(unsigned int mbi_adr)
{
    unsigned int i, j;
    idpde_init(mbi_adr);
    i = 0;
    while(i < num_proc)     
    {
        j = 0;
        while(j < one_k)    
        {
            if (j < 256)    
              set_PDE(i, j);
            else if(j >= 960)
              set_PDE(i, j);
            else
              rmv_PDE(i, j);
            j++;
        }
        i++;
    }
}

unsigned int
pt_alloc_pde(unsigned int proc_index, unsigned int vadr)
{
  unsigned int i;
  unsigned int pi;
  unsigned int pde_index;
  pi = palloc();
  if (pi != 0)
  {
    pt_insert_pde(proc_index, vadr, pi);
    pde_index = vadr / (4096 * 1024);
    i = 0;
    while (i < 1024)        
    {
      rmv_PTE(proc_index, pde_index, i);
      i ++;
    }     
  }       
  return pi;
}

void
pt_free_pde(unsigned int proc_index, unsigned int vadr)
{
  unsigned int pi;
  pi = pt_read_pde(proc_index, vadr);
  pt_rmv_pde(proc_index, vadr);
  pfree(pi / PAGESIZE);
}
