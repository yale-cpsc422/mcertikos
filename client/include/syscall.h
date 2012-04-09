#ifndef CLIENT_SYSCALL_H
#define CLIENT_SYSCALL_H

#include <gcc.h>

// The number of the interrupt on which the syscalls happend
/*
#define T_CLIENT_SYSCALL 48

#define SYSCALL_CLIENT_PUTS 1
#define SYSCALL_CLIENT_GETC 2
#define SYSCALL_CLIENT_TIME 3
#define SYSCALL_CLIENT_PID 4
#define SYSCALL_CLIENT_CPU 5
#define SYSCALL_CLIENT_SETUPVM 6
*/
#include <sys/sys/syscall.h>

void puts(const char *s);
int getc(void); 
int ncpu(void); 
int gettime();
int getpid();
int getcpu();
int cpu_status(int cpu); 

void cpu_signal (void(*f)(void)); 

void cpu_signalret();
void start_vm_client();

#endif
