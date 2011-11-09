#ifndef _H_INTERCEPT_H_
#define _H_INTERCEPT_H_

/* To parse the exit code in case of an Intercept
and perform actions based it */

#include "vm.h"

extern void handle_vmexit (struct vm_info *vm);
void* get_io_data_address(struct vm_info *vm);
#define INTRCPT_ENABLED 1
#define INTRCPT_DISABLED 0

/* [REF] AMD64 manual Vol. 2, Appendix B */

//Flags for CRn access interception
#define _INTRCPT_WRITE_CR0_OFFSET	16
#define INTRCPT_WRITE_CR3			(1 << (_INTRCPT_WRITE_CR0_OFFSET + 3))

//Flags for exception intercept word
// 1 << vector associated with the exception - VMCB + 8h
#define INTRCPT_DB			(1 << 1)	//Debug exception
#define INTRCPT_TS			(1 << 10)	//Invalid TSS
#define INTRCPT_NP			(1 << 11)	//Segment-Not-Present Exception
#define INTRCPT_GP			(1 << 13)	//General protection
#define INTRCPT_PF			(1 << 14)	//Pagefault exception

//Flags for FIRST general intercept word - VMCB + 0Ch
#define INTRCPT_INTR		(1 << 0)
#define INTRCPT_VINTR		(1 << 4)
#define INTRCPT_READTR		(1 << 9)
#define INTRCPT_IRET		(1 << 20)
#define INTRCPT_POPF		(1 << 17)
#define INTRCPT_CPUID		(1 << 18)
#define INTRCPT_INTN		(1 << 21)
#define INTRCPT_HLT		(1 << 24)
#define INTRCPT_IO		(1 << 27)
#define INTRCPT_MSR		(1 << 28)
#define INTRCPT_TASKSWITCH	(1 << 29)
#define INTRCPT_SHUTDOWN	(1 << 31)

//Flags for SECOND general intercept word - VMCB + 010h
#define INTRCPT_VMRUN     (1 << 0)
#define INTRCPT_VMMCALL   (1 << 1)

// Interception request
#define         USER_CMD_ENABLE                 0
#define         USER_CMD_DISABLE                1
#define         USER_CMD_TEST                   9

#define         USER_ITC_SWINT                  1 << 0
#define         USER_ITC_TASKSWITCH             1 << 1
#define         USER_ITC_SYSCALL                1 << 2
#define         USER_ITC_IRET                   1 << 3
#define         USER_SINGLE_STEPPING    1 << 4
#define         USER_UNPACK                             1 << 5
#define         USER_ITC_ALL                    0xFF

#define         USER_TEST_SWITCHMODE    1

//Debug related VMCB settings
#define	DB_SINGLESTEP 1<<14 

//IOIO Intercept Exitinfo1 masks
#define IOIO_TYPE_MASK   1
#define IOIO_STR_MASK    1<<2
#define IOIO_REP_MASK    1<<3
#define IOIO_PORT_MASK   0xFFFF0000
#define IOIO_SIZE_SHIFT	4
#define IOIO_SIZE_MASK	7<<IOIO_SIZE_SHIFT

#define IOIOTYPE(x) ((x)&IOIO_TYPE_MASK)
#define IOIOSIZE(x) ((x)&IOIO_SIZE_MASK)>>IOIO_SIZE_SHIFT
#define IOIOPORT(x) ((x)>>16)

#define SVM_IOIO_IN 0
#define SVM_IOIO_OUT 1

#define SVM_EXIT_VECTOR_MASK 0xFF

#endif
