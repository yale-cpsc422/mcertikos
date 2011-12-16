// Multiprocessor bootstrap definitions.
// See MultiProcessor Specification Version 1.[14]
// This source file adapted from xv6.
// See COPYRIGHT for copyright information.
#ifndef PIOS_KERN_MP_H
#define PIOS_KERN_MP_H

#define MAX_CPU 64

bool mp_init(void);
int mp_ncpu(void);
uint8_t mp_curcpu(void);
//bool mp_booted(int cpu);
void mp_boot(int cpu, void(*f)(void), uint32_t kstack_loc);
void mp_donebooting(void);

void interrupts_init(void);
void interrupts_enable(int irq, int cpunum);
void interrupts_eoi(void);

int get_IRR_lapic();
int get_ISR_lapic();
#endif /* !PIOS_KERN_MP_H */
