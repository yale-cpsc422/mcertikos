#ifndef USER_H
#define USER_H

#include <architecture/types.h>

// The number of the interrupt on which the syscalls happend
#define T_SYSCALL 48

#define SYSCALL_PUTS 1
#define SYSCALL_GETC 2
#define SYSCALL_NCPU 3
#define SYSCALL_CPUSTATUS 4

#define PAGESIZE 4096

#define SYSCALL_SIGNAL 5
#define SYSCALL_SIGNALRET 6

#define SYSCALL_LOAD 7
#define SYSCALL_MGMT 8

#define SYSCALL_SETUPVM 9
#define SYSCALL_SETUPPIOS 10
#define SYSCALL_CREATEVM 11
// MANAGEMENT INTERFACE

typedef enum mgmt_type {
	MGMT_START,     // starts a cpu with a given procid
	MGMT_STOP,      // stops whatever process is running on the cpu
	MGMT_ALLOCPAGE, // allocate a fresh page for a given process
} mgmt_type;

typedef struct {
	uint32_t cpu;
	uint32_t procid;
} mgmt_start;

typedef struct {
	uint32_t cpu;
} mgmt_stop;

typedef struct {
	uint32_t procid;
	uint32_t va;
} mgmt_allocpage;

typedef struct {
	mgmt_type command;
	char params[100]; // can take shape of any of the above param structures
} mgmt_data;



// EVENTS 

typedef enum signal_type {
	SIGNAL_TIMER,
	SIGNAL_PGFLT,
} signal_type;

typedef struct {
	uint32_t cpu;
	uint32_t procid;
	uint32_t fault_addr;
} signal_pgflt;	

typedef struct signal {
	signal_type type;
	char data[];
} signal;

typedef struct signaldesc {
	void(*f)(void);
	signal* s;
} signaldesc;

#endif
