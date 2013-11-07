#ifndef _KERN_X86_H_
#define _KERN_X86_H_

#ifdef _KERN_

#include "types.h"

/* CR0 */
#define CR0_PE		0x00000001	/* Protection Enable */
#define CR0_MP		0x00000002	/* Monitor coProcessor */
#define CR0_EM		0x00000004	/* Emulation */
#define CR0_TS		0x00000008	/* Task Switched */
#define CR0_NE		0x00000020	/* Numeric Errror */
#define CR0_WP		0x00010000	/* Write Protect */
#define CR0_AM		0x00040000	/* Alignment Mask */
#define CR0_PG		0x80000000	/* Paging */

/* CR4 */
#define CR4_PGE		0x00000080	/* Page Global Enable */
#define CR4_OSFXSR	0x00000200	/* SSE and FXSAVE/FXRSTOR enable */
#define CR4_OSXMMEXCPT	0x00000400	/* Unmasked SSE FP exceptions */

/* EFER */
#define MSR_EFER	0xc0000080
# define MSR_EFER_SVME	(1<<12)		/* for AMD processors */

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
void enable_sse(void);

#endif /* _KERN_ */

#endif /* !_KERN_X86_H_ */
