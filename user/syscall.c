#include <inc/gcc.h>
#include <architecture/types.h>

#include <inc/user.h>

void puts(const char *s) {
    asm volatile("int %0" :
            : "i" (T_SYSCALL),
              "a" (SYSCALL_PUTS),
              "b" (s)
            : "cc", "memory");
}

int getc() {
	int c;
    asm volatile("int %0" :
            : "i" (T_SYSCALL),
              "a" (SYSCALL_GETC),
              "b" (&c)
            : "cc", "memory");
	return c;
}

int ncpu() {
	int c;
    asm volatile("int %0" :
            : "i" (T_SYSCALL),
              "a" (SYSCALL_NCPU),
              "b" (&c)
            : "cc", "memory");
	return c;
}


int cpu_status(int cpu) {
	volatile int c;
	c = cpu; 
    asm volatile("int %0" :
            : "i" (T_SYSCALL),
              "a" (SYSCALL_CPUSTATUS),
              "b" (&c)
            : "cc", "memory");
	return c;
}

void cpu_signal(void(*f)(void), signal* buffer) {
	signaldesc sd;
	sd.f = f;
	sd.s = buffer;
    asm volatile("int %0" :
            : "i" (T_SYSCALL),
              "a" (SYSCALL_SIGNAL),
              "b" (&sd)
            : "cc", "memory");
	return;
}

void cpu_signalret() {
    asm volatile("int %0" :
            : "i" (T_SYSCALL),
              "a" (SYSCALL_SIGNALRET),
              "b" (NULL)
            : "cc", "memory");
	return;
}

void progload(char* exe, uint32_t* procid) {
    asm volatile("int %0" :
            : "i" (T_SYSCALL),
              "a" (SYSCALL_LOAD),
              "b" (exe),
			  "c" (procid)
            : "cc", "memory");
	return;
}

void mgmt(mgmt_data* data) {
    asm volatile("int %0" :
            : "i" (T_SYSCALL),
              "a" (SYSCALL_MGMT),
              "b" (data)
            : "cc", "memory");
	return;
}

void cpustart(uint32_t cpu, uint32_t procid) {
	mgmt_start params = {cpu, procid};
	mgmt_data data;
	data.command = MGMT_START;
	*((mgmt_start*)&data.params) = params;
	mgmt(&data);
	return;
}

void cpustop(uint32_t cpu) {
	mgmt_stop params = {cpu};
	mgmt_data data;
	data.command = MGMT_STOP;
	*((mgmt_stop*)&data.params) = params;
	mgmt(&data);
	return;
}

void allocpage(uint32_t procid, uint32_t va) {
	mgmt_allocpage params = {procid, va};
	mgmt_data data;
	data.command = MGMT_ALLOCPAGE;
	*((mgmt_allocpage*)&data.params) = params;
	mgmt(&data);
	return;
}

/*void createvm() {
    asm volatile("int %0" :
            : "i" (T_SYSCALL),
              "a" (SYSCALL_CREATEVM),
              "b" (NULL)
            : "cc", "memory");
	return;
}*/


void createvm( uint32_t* procid) {
    asm volatile("int %0" :
            : "i" (T_SYSCALL),
              "a" (SYSCALL_CREATEVM),
			  "b" (procid)
            : "cc", "memory");
	return;
}

void setupvm() {
    asm volatile("int %0" :
            : "i" (T_SYSCALL),
              "a" (SYSCALL_SETUPVM),
              "b" (NULL)
            : "cc", "memory");
	return;
}
void setuppios() {
    asm volatile("int %0" :
            : "i" (T_SYSCALL),
              "a" (SYSCALL_SETUPPIOS),
              "b" (NULL)
            : "cc", "memory");
	return;
}
