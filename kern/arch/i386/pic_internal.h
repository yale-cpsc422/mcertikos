// Hardware definitions for the 8259A Programmable Interrupt Controller (PIC).
// See COPYRIGHT for copyright information.
#ifndef PIOS_DEV_PIC_INTERNAL_H
#define PIOS_DEV_PIC_INTERNAL_H

#define MAX_IRQS	16	// Number of IRQs

// I/O Addresses of the two 8259A programmable interrupt controllers
#define IO_PIC1		0x20	// Master (IRQs 0-7)
#define IO_PIC2		0xA0	// Slave (IRQs 8-15)

#define IRQ_SLAVE	2	// IRQ at which slave connects to master


#ifndef __ASSEMBLER__
#include <inc/arch/types.h>
extern uint16_t irq_mask_8259A;
#endif // !__ASSEMBLER__

#endif // !PIOS_DEV_PIC_INTERNAL_H
