#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include <inc/user.h>
#include <inc/gcc.h>
#include <architecture/types.h>

void puts(const char *s);
int getc(void); 
int ncpu(void); 
int cpu_status(int cpu); 

void cpu_signal (void(*f)(void), signal* buffer); 

void cpu_signalret();

void progload(char* exe, uint32_t* procid);
void cpustart(uint32_t cpu, uint32_t procid);
void cpustop(uint32_t cpu);
void allocpage(uint32_t procid, uint32_t va);

void setupvm();
#endif
