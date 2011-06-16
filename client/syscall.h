#ifndef CLIENT_SYSCALL_H
#define CLIENT_SYSCALL_H

#include <inc/gcc.h>

void puts(const char *s);
int getc(void); 
int ncpu(void); 
int gettime();
int getpid();
int getcpu();
int cpu_status(int cpu); 

void cpu_signal (void(*f)(void)); 

void cpu_signalret();

#endif
