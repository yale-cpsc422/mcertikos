#ifndef CERTIKOS_KERN_APIC_H
#define CERTIKOS_KERN_APIC_H

#include <architecture/types.h>

#define HZ	25

/*
 * Local APIC
 */

#define LAPIC_ID		(0x0020/4)   // ID
#define LAPIC_VER		(0x0030/4)   // Version
#define LAPIC_TPR		(0x0080/4)   // Task Priority
#define LAPIC_APR		(0x0090/4)   // Arbitration Priority
#define LAPIC_PPR		(0x00A0/4)   // Processor Priority
#define LAPIC_EOI		(0x00B0/4)   // EOI
#define LAPIC_LDR		(0x00D0/4)   // Logical Destination
#define LAPIC_DFR		(0x000E0/4)  // Destination Format
# define LAPIC_DFR_FLAT_MODE	(0xF << 28)  // Flat Mode
#define LAPIC_SVR		(0x00F0/4)   // Spurious Interrupt Vector
# define LAPIC_SVR_ENABLE	0x00000100   // Unit Enable
#define LAPIC_ISR		(0x0100/4)   //ISR
#define LAPIC_IRR		(0x0200/4)   //IRR
#define LAPIC_ESR		(0x0280/4)   // Error Status
#define LAPIC_ICRLO		(0x0300/4)   // Interrupt Command
# define LAPIC_ICRLO_INIT       0x00000500   // INIT/RESET
# define LAPIC_ICRLO_STARTUP    0x00000600   // Startup IPI
# define LAPIC_ICRLO_DELIVS     0x00001000   // Delivery status
# define LAPIC_ICRLO_ASSERT     0x00004000   // Assert interrupt (vs deassert)
# define LAPIC_ICRLO_LEVEL      0x00008000   // Level triggered
# define LAPIC_ICRLO_BCAST      0x00080000   // Send to all APICs, including self.
#define LAPIC_ICRHI		(0x0310/4)   // Interrupt Command [63:32]
#define LAPIC_TIMER		(0x0320/4)   // Local Vector Table 0 (TIMER)
# define LAPIC_TIMER_X1		0x0000000B   // divide counts by 1
# define LAPIC_TIMER_PERIODIC   0x00020000   // Periodic
#define LAPIC_PCINT		(0x0340/4)   // Performance Counter LVT
#define LAPIC_LINT0		(0x0350/4)   // Local Vector Table 1 (LINT0)
#define LAPIC_LINT1		(0x0360/4)   // Local Vector Table 2 (LINT1)
#define LAPIC_ERROR		(0x0370/4)   // Local Vector Table 3 (ERROR)
# define LAPIC_ERROR_MASKED     0x00010000   // Interrupt masked
#define LAPIC_TICR		(0x0380/4)   // Timer Initial Count
#define LAPIC_TCCR		(0x0390/4)   // Timer Current Count
#define LAPIC_TDCR		(0x03E0/4)   // Timer Divide Configuration

#define LAPIC_MASKED		0x00001000

/*
 * IOAPIC
 */

#define IOAPIC_DEFAULT		0xFEC00000   // Default physical address of IO APIC

#define IOAPIC_ID		0x00         // Register index: ID
#define IOAPIC_VER		0x01         // Register index: version
#define IOAPIC_TABLE		0x10         // Redirection table base

// The redirection table starts at REG_TABLE and uses
// two registers to configure each interrupt.
// The first (low) register in a pair contains configuration bits.
// The second (high) register contains a bitmask telling which
// CPUs can serve that interrupt.
#define IOAPIC_INT_DISABLED	0x00010000  // Interrupt disabled
#define IOAPIC_INT_LEVEL	0x00008000  // Level-triggered (vs edge-)
#define IOAPIC_INT_ACTIVELOW	0x00002000  // Active low (vs high)
#define IOAPIC_INT_LOGICAL	0x00000800  // Destination is CPU id (vs APIC ID)


typedef uint32_t lapic_t;

typedef uint8_t lapicid_t;

typedef volatile
struct ioapic {
	uint32_t reg;
	uint32_t pad[3];
	uint32_t data;
} ioapic_t;

volatile lapic_t	*lapic;

void lapic_init(void);
void lapic_eoi(void);
void lapic_startcpu(lapicid_t, uintptr_t);

int lapic_get_irr(void);
int lapic_get_isr(void);

void ioapic_init(void);
void ioapic_enable(int irqno, lapicid_t);
void ioapic_register(uintptr_t base, lapic_t, int gsi);

#endif /* !CERTIKOS_KERN_APIC_H */
