#ifndef _KERN_X86_H_
#define _KERN_X86_H_

#ifdef _KERN_

#include "types.h"

/* EFLAGS register */
#define FL_CF		0x00000001	/* Carry Flag */
#define FL_PF		0x00000004	/* Parity Flag */
#define FL_AF		0x00000010	/* Auxiliary carry Flag */
#define FL_ZF		0x00000040	/* Zero Flag */
#define FL_SF		0x00000080	/* Sign Flag */
#define FL_TF		0x00000100	/* Trap Flag */
#define FL_IF		0x00000200	/* Interrupt Flag */
#define FL_DF		0x00000400	/* Direction Flag */
#define FL_OF		0x00000800	/* Overflow Flag */
#define FL_IOPL_MASK	0x00003000	/* I/O Privilege Level bitmask */
#define FL_IOPL_0	0x00000000	/*   IOPL == 0 */
#define FL_IOPL_1	0x00001000	/*   IOPL == 1 */
#define FL_IOPL_2	0x00002000	/*   IOPL == 2 */
#define FL_IOPL_3	0x00003000	/*   IOPL == 3 */
#define FL_NT		0x00004000	/* Nested Task */
#define FL_RF		0x00010000	/* Resume Flag */
#define FL_VM		0x00020000	/* Virtual 8086 mode */
#define FL_AC		0x00040000	/* Alignment Check */
#define FL_VIF		0x00080000	/* Virtual Interrupt Flag */
#define FL_VIP		0x00100000	/* Virtual Interrupt Pending */
#define FL_ID		0x00200000	/* ID flag */

/* CR0 */
#define CR0_PE		0x00000001	/* Protection Enable */
#define CR0_MP		0x00000002	/* Monitor coProcessor */
#define CR0_EM		0x00000004	/* Emulation */
#define CR0_TS		0x00000008	/* Task Switched */
#define CR0_ET		0x00000010	/* Extension Type */
#define CR0_NE		0x00000020	/* Numeric Errror */
#define CR0_WP		0x00010000	/* Write Protect */
#define CR0_AM		0x00040000	/* Alignment Mask */
#define CR0_NW		0x20000000	/* Not Writethrough */
#define CR0_CD		0x40000000	/* Cache Disable */
#define CR0_PG		0x80000000	/* Paging */

/* CR4 */
#define CR4_VME		0x00000001	/* V86 Mode Extensions */
#define CR4_PVI		0x00000002	/* Protected-Mode Virtual Interrupts */
#define CR4_TSD		0x00000004	/* Time Stamp Disable */
#define CR4_DE		0x00000008	/* Debugging Extensions */
#define CR4_PSE		0x00000010	/* Page Size Extensions */
#define CR4_PAE		0x00000020	/* Physical Address Extension */
#define CR4_MCE		0x00000040	/* Machine Check Enable */
#define CR4_PGE		0x00000080	/* Page Global Enable */
#define CR4_PCE		0x00000100	/* Performance counter enable */
#define CR4_OSFXSR	0x00000200	/* SSE and FXSAVE/FXRSTOR enable */
#define CR4_OSXMMEXCPT	0x00000400	/* Unmasked SSE FP exceptions */

/* EFER */
#define MSR_EFER	0xc0000080
# define MSR_EFER_SCE	(1<<0)
# define MSR_EFER_LME	(1<<8)
# define MSR_EFER_LMA	(1<<10)
# define MSR_EFER_NXE	(1<<11)
# define MSR_EFER_SVME	(1<<12)		/* for AMD processors */
# define MSR_EFER_LMSLE	(1<<13)
# define MSR_EFER_FFXSR	(1<<14)

/* Other MSRs */
#define MSR_IA32_SYSENTER_CS		0x174
#define MSR_IA32_SYSENTER_ESP		0x175
#define MSR_IA32_SYSENTER_EIP		0x176
#define MSR_PAT				0x277
#define MSR_IA32_PERF_GLOBAL_CTRL	0x38f

/* CPUID */
/* 0x0000_0001 ECX */
#define CPUID_FEATURE_RDRAND		(1<<21)
#define CPUID_FEATURE_F16C		(1<<29)
#define CPUID_FEATURE_AVX		(1<<28)
#define CPUID_FEATURE_OSXSAVE		(1<<27)
#define CPUID_FEATURE_XSAVE		(1<<26)
#define CPUID_FEATURE_AES		(1<<25)
#define CPUID_FEATURE_TSC_DEADLINE	(1<<24)
#define CPUID_FEATURE_POPCNT		(1<<23)
#define CPUID_FEATURE_MOVBE		(1<<22)
#define CPUID_FEATURE_X2APIC		(1<<21)
#define CPUID_FEATURE_SSE42		(1<<20)
#define CPUID_FEATURE_SSE41		(1<<19)
#define CPUID_FEATURE_DCA		(1<<18)
#define CPUID_FEATURE_PCID		(1<<17)
#define CPUID_FEATURE_PDCM		(1<<15)
#define CPUID_FEATURE_XTPR		(1<<14)
#define CPUID_FEATURE_CMPXCHG16B	(1<<13)
#define CPUID_FEATURE_FMA		(1<<12)
#define CPUID_FEATURE_CNXT_ID		(1<<10)
#define CPUID_FEATURE_SSSE3		(1<<9)
#define CPUID_FEATURE_TM2		(1<<8)
#define CPUID_FEATURE_EIST		(1<<7)
#define CPUID_FEATURE_SMX		(1<<6)
#define CPUID_FEATURE_VMX		(1<<5)
#define CPUID_FEATURE_DS_CPL		(1<<4)
#define CPUID_FEATURE_MONITOR		(1<<3)
#define CPUID_FEATURE_DTES64		(1<<2)
#define CPUID_FEATURE_PCLMULQDQ		(1<<1)
#define CPUID_FEATURE_SSE3		(1<<0)
/* 0x0000_0001 EDX */
#define CPUID_FEATURE_PBE		(1<<31)
#define CPUID_FEATURE_TM		(1<<29)
#define CPUID_FEATURE_HTT		(1<<28)
#define CPUID_FEATURE_SS		(1<<27)
#define CPUID_FEATURE_SSE2		(1<<26)
#define CPUID_FEATURE_SSE		(1<<25)
#define CPUID_FEATURE_FXSR		(1<<24)
#define CPUID_FEATURE_MMX		(1<<23)
#define CPUID_FEATURE_ACPI		(1<<22)
#define CPUID_FEATURE_DS		(1<<21)
#define CPUID_FEATURE_CLFUSH		(1<<19)
#define CPUID_FEATURE_PSE36		(1<<17)
#define CPUID_FEATURE_PAT		(1<<16)
#define CPUID_FEATURE_CMOV		(1<<15)
#define CPUID_FEATURE_MCA		(1<<14)
#define CPUID_FEATURE_PGE		(1<<13)
#define CPUID_FEATURE_MTRR		(1<<12)
#define CPUID_FEATURE_SYSENTREXIT	(1<<11)
#define CPUID_FEATURE_APIC		(1<<9)
#define CPUID_FEATURE_CMPXCHG8B		(1<<8)
#define CPUID_FEATURE_MCE		(1<<7)
#define CPUID_FEATURE_PAE		(1<<6)
#define CPUID_FEATURE_MSR		(1<<5)
#define CPUID_FEATURE_TSC		(1<<4)
#define CPUID_FEATURE_PSE		(1<<3)
#define CPUID_FEATURE_DE		(1<<2)
#define CPUID_FEATURE_VME		(1<<1)
#define CPUID_FEATURE_FPU		(1<<0)
/* 0x8000_0001 ECX */
#define CPUID_X_FEATURE_TOP		(1<<22)
#define CPUID_X_FEATURE_TBM		(1<<21)
#define CPUID_X_FEATURE_NODEID		(1<<19)
#define CPUID_X_FEATURE_FMA4		(1<<16)
#define CPUID_X_FEATURE_LWP		(1<<15)
#define CPUID_X_FEATURE_WDT		(1<<13)
#define CPUID_X_FEATURE_SKINIT		(1<<12)
#define CPUID_X_FEATURE_XOP		(1<<11)
#define CPUID_X_FEATURE_IBS		(1<<10)
#define CPUID_X_FEATURE_OSVW		(1<<9)
#define CPUID_X_FEATURE_PREFETCH	(1<<8)
#define CPUID_X_FEATURE_MISALIGN	(1<<7)
#define CPUID_X_FEATURE_SSE4A		(1<<6)
#define CPUID_X_FEATURE_ABM		(1<<5)
#define CPUID_X_FEATURE_ALTMOV		(1<<4)
#define CPUID_X_FEATURE_XAPIC		(1<<3)
#define CPUID_X_FEATURE_SVM		(1<<2)
#define CPUID_X_FEATURE_CMP		(1<<1)
#define CPUID_X_FEATURE_LAHF		(1<<0)
/* 0x8000_00001 EDX */
#define CPUID_X_FEATURE_3DNOW		(1<<31)
#define CPUID_X_FEATURE_X3DNOW		(1<<30)
#define CPUID_X_FEATURE_LM		(1<<29)
#define CPUID_X_FEATURE_RDTSCP		(1<<27)
#define CPUID_X_FEATURE_1GPG		(1<<26)
#define CPUID_X_FEATURE_FFXSR		(1<<25)
#define CPUID_X_FEATURE_FXSR		(1<<24)
#define CPUID_X_FEATURE_MMX		(1<<23)
#define CPUID_X_FEATURE_XMMX		(1<<22)
#define CPUID_X_FEATURE_NX		(1<<20)
#define CPUID_X_FEATURE_PSE36		(1<<17)
#define CPUID_X_FEATURE_PAT		(1<<16)
#define CPUID_X_FEATURE_CMOV		(1<<15)
#define CPUID_X_FEATURE_MCA		(1<<14)
#define CPUID_X_FEATURE_PGE		(1<<13)
#define CPUID_X_FEATURE_MTRR		(1<<12)
#define CPUID_X_FEATURE_SYSCALL		(1<<11)
#define CPUID_X_FEATURE_APIC		(1<<9)
#define CPUID_X_FEATURE_CMPXHG8B	(1<<8)
#define CPUID_X_FEATURE_MCE		(1<<7)
#define CPUID_X_FEATURE_PAE		(1<<6)
#define CPUID_X_FEATURE_MSR		(1<<5)
#define CPUID_X_FEATURE_TSC		(1<<4)
#define CPUID_X_FEATURE_PSE		(1<<3)
#define CPUID_X_FEATURE_DE		(1<<2)
#define CPUID_X_FEATURE_VME		(1<<1)
#define CPUID_X_FEATURE_FPU		(1<<0)

uint32_t read_ebp(void);
void lldt(uint16_t sel);
uint16_t rldt(void);
void ltr(uint16_t sel);
void lcr0(uint32_t val);
uint32_t rcr0(void);
uint32_t rcr2(void);
void lcr3(uint32_t val);
void lcr4(uint32_t val);
uint32_t rcr4(void);
void cpuid(uint32_t info,
	   uint32_t *eaxp, uint32_t *ebxp, uint32_t *ecxp, uint32_t *edxp);
void cli(void);
void sti(void);
uint64_t rdmsr(uint32_t msr);
void wrmsr(uint32_t msr, uint64_t newval);
void cld(void);
uint8_t inb(int port);
uint16_t inw(int port);
uint32_t inl(int port);
void insl(int port, void *addr, int cnt);
void outb(int port, uint8_t data);
void outw(int port, uint16_t data);
void outsw(int port, const void *addr, int cnt);
void outl(int port, uint32_t dat);
void halt(void);
void pause(void);
uint64_t rdtsc(void);

#endif /* _KERN_ */

#endif /* !_KERN_X86_H_ */
