#ifndef _KERN_PROC_ELF_H_
#define _KERN_PROC_ELF_H_

#ifdef _KERN_

#include <mm/export.h>

void elf_load(uintptr_t user_elf_addr, pmap_t *pmap);
uintptr_t elf_entry(uintptr_t user_elf_addr);

#endif /* _KERN_ */

#endif /* _KERN_PROC_ELF_H_ */
