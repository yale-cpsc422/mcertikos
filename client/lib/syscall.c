#include <gcc.h>
#include <types.h>

#include <syscall.h>

void puts(const char *s) {
    asm volatile("int %0" :
            : "i" (T_CLIENT_SYSCALL),
              "a" (SYSCALL_CLIENT_PUTS),
              "b" (s)
            : "cc", "memory");
}

int getc() {
	int c;
    asm volatile("int %0" :
            : "i" (T_CLIENT_SYSCALL),
              "a" (SYSCALL_CLIENT_GETC),
              "b" (&c)
            : "cc", "memory");
	return c;
}

int gettime() {
	int t;
    asm volatile("int %0" :
            : "i" (T_CLIENT_SYSCALL),
              "a" (SYSCALL_CLIENT_TIME),
              "b" (&t)
            : "cc", "memory");
	return t;
}
	
int getpid() {
	int t;
    asm volatile("int %0" :
            : "i" (T_CLIENT_SYSCALL),
              "a" (SYSCALL_CLIENT_PID),
              "b" (&t)
            : "cc", "memory");
	return t;
}
	
int getcpu() {
	int t;
    asm volatile("int %0" :
            : "i" (T_CLIENT_SYSCALL),
              "a" (SYSCALL_CLIENT_CPU),
              "b" (&t)
            : "cc", "memory");
	return t;
}
void start_vm_client(){
asm volatile("int %0" :
            : "i" (T_CLIENT_SYSCALL),
              "a" (SYSCALL_CLIENT_SETUPVM),
              "b" (NULL)
            : "cc", "memory");
}
