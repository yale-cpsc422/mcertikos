#ifndef __SVM_H__
#define __SVM_H__

//#include "types.h"
#include <architecture/types.h>
#include <architecture/cpu.h>
#include "vmcb.h"

#ifndef __ASSEMBLY__

// - variables to store and load registers of Guest
// we don't need to store eax because eax is stored inside vmcb.rax
// and it will be loaded / stored with VMLOAD / VMSAVE (remember to use these instructions)
uint32_t g_ebp;
uint32_t g_ebx;
uint32_t g_ecx;
uint32_t g_edx;
uint32_t g_esi;
uint32_t g_edi;

extern void  enable_svm ( struct cpuinfo_x86 *c );
extern void svm_launch ();
extern void  enable_amd_svm ( void );


#endif

#endif /* __SVM_H__ */
