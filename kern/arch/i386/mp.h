// Multiprocessor bootstrap definitions.
// See MultiProcessor Specification Version 1.[14]
// This source file adapted from xv6.
// See COPYRIGHT for copyright information.
#ifndef PIOS_KERN_MP_H
#define PIOS_KERN_MP_H

#include <architecture/apic.h>

#define MAX_CPU 64

bool mp_init(void);
int mp_ncpu(void);
uint8_t mp_curcpu(void);
//bool mp_booted(int cpu);
void mp_boot(int cpu, void(*f)(void), uint32_t kstack_loc);
void mp_donebooting(void);
bool mp_ismp(void);
lapicid_t mp_cpuid(int);

#endif /* !PIOS_KERN_MP_H */
