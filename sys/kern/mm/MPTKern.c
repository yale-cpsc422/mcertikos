#include "MPTComm.h"

#define MagicNumber 1048577
#define PAGESIZE 4096

void
pt_init_kern(unsigned int mbi_adr)
{
    unsigned int i;
    pt_init_comm(mbi_adr);
    i = 256;
    while(i < 960)
    {
        set_PDE(0, i);
        i ++;
    }
}

unsigned int
pt_insert(unsigned int proc_index, unsigned int vadr, unsigned int padr, unsigned int perm)
{   
  unsigned int pi; 
  unsigned int result;
  unsigned int count;
  pi = pt_read_pde(proc_index, vadr);
  if (pi != 0)
    result = 0;
  else
  {
    result = pt_alloc_pde(proc_index, vadr);
    if (result == 0)
      result = MagicNumber;
  }
  if (result != MagicNumber)
  {
    pt_insert_aux(proc_index, vadr, padr, perm);
    count = at_get_c(padr);
    at_set_c(padr, count + 1);
  }
  return result;
}

unsigned int
pt_rmv(unsigned int proc_index, unsigned int vadr)
{
  unsigned int padr;
  unsigned int count;
  padr = pt_read(proc_index, vadr);
  if (padr != 0)
  {
    pt_rmv_aux(proc_index, vadr);
    count = at_get_c(padr / PAGESIZE);
    if (count > 0)
      at_set_c(padr / PAGESIZE, count - 1);
  }
  return padr;
}   

