// Hardware definitions for the 8259A Programmable Interrupt Controller (PIC).
// See COPYRIGHT for copyright information.
#ifndef PIOS_DEV_PIC_H
#define PIOS_DEV_PIC_H

#include <architecture/types.h>

void pic_init(void);
void pic_setmask(uint16_t mask);
void pic_enable(int irq);
void pic_eoi(void);

#endif // !PIOS_DEV_PIC_H
