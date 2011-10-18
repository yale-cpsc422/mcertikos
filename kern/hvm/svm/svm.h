#ifndef __SVM_H__
#define __SVM_H__

#include <architecture/types.h>
#include <architecture/cpu.h>
#include "vmcb.h"

#ifndef __ASSEMBLY__

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
