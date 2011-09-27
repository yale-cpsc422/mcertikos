// Multiprocessor bootstrap definitions.
// See MultiProcessor Specification Version 1.[14]
// This source file adapted from xv6.
// See COPYRIGHT for copyright information.
#ifndef PIOS_KERN_MP_INTERNAL_H
#define PIOS_KERN_MP_INTERNAL_H

#define HZ 	25

// Local APIC registers, divided by 4 for use as uint32_t[] indices.
#define ID      (0x0020/4)   // ID
#define VER     (0x0030/4)   // Version
#define TPR     (0x0080/4)   // Task Priority
#define APR     (0x0090/4)      // Arbitration Priority
#define PPR     (0x00A0/4)      // Processor Priority
#define EOI     (0x00B0/4)      // EOI
#define LDR     (0x00D0/4)      // Logical Destination
#define DFR     (0x000E0/4)     // Destination Format
#define SVR     (0x00F0/4)   // Spurious Interrupt Vector
  #define ENABLE     0x00000100   // Unit Enable
#define ESR     (0x0280/4)   // Error Status
#define ICRLO   (0x0300/4)   // Interrupt Command
  #define INIT       0x00000500   // INIT/RESET
  #define STARTUP    0x00000600   // Startup IPI
  #define DELIVS     0x00001000   // Delivery status
  #define ASSERT     0x00004000   // Assert interrupt (vs deassert)
  #define LEVEL      0x00008000   // Level triggered
  #define BCAST      0x00080000   // Send to all APICs, including self.
#define ICRHI   (0x0310/4)   // Interrupt Command [63:32]
#define TIMER   (0x0320/4)   // Local Vector Table 0 (TIMER)
  #define X1         0x0000000B   // divide counts by 1
  #define PERIODIC   0x00020000   // Periodic
#define PCINT   (0x0340/4)   // Performance Counter LVT
#define LINT0   (0x0350/4)   // Local Vector Table 1 (LINT0)
#define LINT1   (0x0360/4)   // Local Vector Table 2 (LINT1)
#define ERROR   (0x0370/4)   // Local Vector Table 3 (ERROR)
  #define MASKED     0x00010000   // Interrupt masked
#define TICR    (0x0380/4)   // Timer Initial Count
#define TCCR    (0x0390/4)   // Timer Current Count
#define TDCR    (0x03E0/4)   // Timer Divide Configuration



struct mp {            	// MP floating pointer structure
	uint8_t signature[4];		// "_MP_"
	void *physaddr;			// phys addr of MP config table
	uint8_t length;			// 1
	uint8_t specrev;		// [14]
	uint8_t checksum;		// all bytes must add up to 0
	uint8_t type;			// MP system config type
	uint8_t imcrp;
	uint8_t reserved[3];
};

struct mpconf {         // configuration table header
	uint8_t signature[4];		// "PCMP"
	uint16_t length;		// total table length
	uint8_t version;		// [14]
	uint8_t checksum;		// all bytes must add up to 0
	uint8_t product[20];		// product id
	uint32_t *oemtable;		// OEM table pointer
	uint16_t oemlength;		// OEM table length
	uint16_t entry;			// entry count
	uint32_t *lapicaddr;		// address of local APIC
	uint16_t xlength;		// extended table length
	uint8_t xchecksum;		// extended table checksum
	uint8_t reserved;
};

struct mpproc {         // processor table entry
	uint8_t type;			// entry type (0)
	uint8_t apicid;			// local APIC id
	uint8_t version;		// local APIC version
	uint8_t flags;			// CPU flags
	  #define MPENAB 0x01		// This processor is enabled.
	  #define MPBOOT 0x02           // This proc is the bootstrap processor.
	uint8_t signature[4];		// CPU signature
	uint32_t feature;		// feature flags from CPUID instruction
	uint8_t reserved[8];
};

struct mpioapic {       // I/O APIC table entry
	uint8_t type;			// entry type (2)
	uint8_t apicno;			// I/O APIC id
	uint8_t version;		// I/O APIC version
	uint8_t flags;			// I/O APIC flags
	uint32_t *addr;			// I/O APIC address
};

// IO APIC MMIO structure: write reg, then read or write data.
struct ioapic {
	uint32_t reg;
	uint32_t pad[3];
	uint32_t data;
};

// Table entry types
#define MPPROC    0x00  // One per processor
#define MPBUS     0x01  // One per bus
#define MPIOAPIC  0x02  // One per I/O APIC
#define MPIOINTR  0x03  // One per bus interrupt source
#define MPLINTR   0x04  // One per system interrupt source


// System information gleaned by mp_init()
// extern int ismp;		// True if this is an MP-capable system
// extern int ncpu;		// Total number of CPUs found
// extern uint8_t ioapicid;	// APIC ID of system's I/O APIC
// extern volatile struct ioapic *ioapic;	// Address of I/O APIC

//#define IOAPIC  0xFEC00000   // Default physical address of IO APIC

#define REG_ID     0x00  // Register index: ID
#define REG_VER    0x01  // Register index: version
#define REG_TABLE  0x10  // Redirection table base

// The redirection table starts at REG_TABLE and uses
// two registers to configure each interrupt.  
// The first (low) register in a pair contains configuration bits.
// The second (high) register contains a bitmask telling which
// CPUs can serve that interrupt.
#define INT_DISABLED   0x00010000  // Interrupt disabled
#define INT_LEVEL      0x00008000  // Level-triggered (vs edge-)
#define INT_ACTIVELOW  0x00002000  // Active low (vs high)
#define INT_LOGICAL    0x00000800  // Destination is CPU id (vs APIC ID)


void lapic_startcpu(uint8_t apicid, uint32_t addr);
void ioapic_init(void);
void ioapic_enable(int irq, int apicid);

// Initialize current CPU's local APIC
void lapic_init(void);
// Initialize current CPU's local APIC
void lapic_init2(void);

// Acknowledge interrupt
void lapic_eoi(void);

static void lapicw(int index, int value);
void microdelay(int us);

#endif /* !PIOS_KERN_MP_H */
