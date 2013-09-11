#ifndef _KERN_PROC_ELF_H_
#define _KERN_PROC_ELF_H_

#ifdef _KERN_

void elf_load(uintptr_t user_elf_addr, int pmap_id);
uintptr_t elf_entry(uintptr_t user_elf_addr);

#endif /* _KERN_ */

#endif /* _KERN_PROC_ELF_H_ */
