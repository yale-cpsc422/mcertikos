/*
 * Hardware definitions for the 8259A Programmable Interrupt Controller (PIC).
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from the MIT Exokernel and JOS.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */

#ifndef _KERN_DEV_PIC_H_
#define _KERN_DEV_PIC_H_

#ifndef _KERN_
#error "This is a kernel header file; do not include it in userspace programs."
#endif

#ifndef __ASSEMBLER__

#include <lib/types.h>
#include <lib/x86.h>

extern uint16_t irq_mask_8259A;

void pic_init(void);
void pic_setmask(uint16_t mask);
void pic_enable(int irq);
void pic_eoi(void);
void pic_reset(void);

#endif // !__ASSEMBLER__

#endif // !_KERN_DEV_PIC_H_
