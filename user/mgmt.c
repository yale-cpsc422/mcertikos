#include <architecture/types.h>

#include <inc/user.h>
#include <user/stdio.h>
#include <user/string.h>
#include <user/syscall.h>
#include <inc/elf.h>

#define MAX_PROG 10
#define MAX_PROGNAME 50
#define MAX_PROCS 50

#define assert(x)		\
	do { if (!(x)) {printf("assertion failed: %s", #x); while(1);}} while (0)

int cpus;

extern char _binary_obj_client_client1_start[];
extern char _binary_obj_client_vmclient_start[];
extern char _binary_obj_client_evilclient_start[];

struct program {
	char progname[MAX_PROGNAME];
	char* prog_ptr;
};

struct program programs[] =
{
	{"client1", _binary_obj_client_client1_start},
	{"vmclient", _binary_obj_client_vmclient_start},
	{"evilclient", _binary_obj_client_evilclient_start},
	{"", (char*)0}
};

static int prog_num = 3;
static int waiting_for_input;

struct proc_info {
	uint32_t pid; // 0 = empty
	char progname[MAX_PROGNAME];
	int cpu; // -1 = not active
};

static struct proc_info procs[MAX_PROCS];
static int cpu_procs[255]; // stores the proc_index of the process running on that CPU

uint32_t pid;

void performCmd(char* cmd) {
	int i;
	if (strcmp(cmd,"status") == 0) {
		printf("There are %d cpus detected\n", cpus);
		printf("CPU states: ");
		for (i=0;i<cpus;i++) {
			printf("%d ", cpu_status(i));
		}
		printf("\n");
	}
	else if (strcmp(cmd,"list") == 0) {
		i=0;
		printf("The mgmt program can load the following programs:\n");
		while(programs[i].prog_ptr) {
			printf("%02d. %s\n", i, programs[i].progname);
			i++;
		}
	}
	else if (strncmp(cmd,"ps", 2) == 0) {
		int found=0;
		for (i=0;i<MAX_PROCS;i++) {
			if (procs[i].pid) {
				found = 1;
				if (procs[i].cpu == -1)
					printf("%02d. %s (not running)\n", procs[i].pid, procs[i].progname);
				else
					printf("%02d. %s (running on cpu %d)\n", procs[i].pid, procs[i].progname, procs[i].cpu);
			}
		}
		if (!found)
			printf("No processes exist\n");
	}
	else if (strncmp(cmd,"load", 4) == 0) {
		int program;
		if (atoi(cmd+5, &program)==0) {
			printf("Usage: load <program number> - loads a program\n");
			return;
		}
		if (!(0<=program && program < prog_num)) {
		   printf("Program number %d does not exist\n", program);
		   return;
		}
		// search for an empty proc entry
		int proc_index;
		for (proc_index=0;proc_index < MAX_PROCS; proc_index++) {
			if (!procs[proc_index].pid) break;
		}
		if (proc_index == MAX_PROCS) {
			printf("Management program can not handle any more processes (max %d)\n", MAX_PROCS);
			return;
		}
		printf("Loading program \"%s\"\n", programs[program].progname);
		progload(programs[program].prog_ptr, &procs[proc_index].pid);
		if (procs[proc_index].pid) {
			strncpy(procs[proc_index].progname, programs[program].progname, MAX_PROGNAME);
			procs[proc_index].cpu = -1;
			printf("Program loaded, pid %d\n", procs[proc_index].pid);
		}
		else {
			printf("Failed to load the program\n");
		}
	}
	else if (strncmp(cmd,"start",5) == 0) {
		int cpu=0;
		int pid=0;
		int numsize;
		if ((numsize = atoi(cmd+6, &cpu))==0) {
			printf("Usage: start <cpunum> <procid> - starts process procid on cpu <cpunum>\n");
			return;
		}
		if (!atoi(cmd+6+numsize+1, &pid)) {
			printf("Usage: start <cpunum> <procid> - starts process procid on cpu <cpunum>\n");
			return;
		}
		int proc_index;
		for (proc_index=0;proc_index<MAX_PROCS;proc_index++) {
			if (procs[proc_index].pid == pid) break;
		}
		if (proc_index == MAX_PROCS) {
			printf("Process with id %d does not exist\n", pid);
			return;
		}
		if (procs[proc_index].cpu != -1) {
			printf("Process is already running on CPU %d\n", procs[proc_index].cpu);
		}
		if (!(1 <= cpu && cpu < cpus)) {
			printf("Cpu %d can not be used, only cpus 1-%d are valid\n", cpu, cpus-1);
			return;
		}
		if (cpu_procs[cpu] != -1) {
			printf("CPU %d is already in use by process %d\n", cpu, procs[cpu_procs[cpu]].pid);
			return;
		}
		procs[proc_index].cpu = cpu;
		cpu_procs[cpu] = proc_index;
		cpustart(cpu,pid);
		// I should figure out a way to check that the process is running....


		// syscall may not succeed - but the kernel will be ok
	}
	else if (strncmp(cmd,"stop",4) == 0) {
		int cpu=0;
		int numsize;
		int proc_index;
		if ((numsize = atoi(cmd+5, &cpu))==0) {
			printf("Usage: stop <cpunum> - stops running process on cpu <cpunum>\n");
			return;
		}
		if (!(1 <= cpu && cpu < cpus)) {
			printf("Cpu %d can not be used, only cpus 1-%d are valid\n", cpu, cpus-1);
			return;
		}
		proc_index = cpu_procs[cpu];
		if (proc_index == -1) {
			printf("Cpu %d is already idle\n", cpu);
			return;
		}
		cpu_procs[cpu] = -1;
		if (!procs[proc_index].pid) {
			printf("MGMT FAILURE: cpu %d is running a process which does not exist\n", cpu);
			return;
		}
		procs[proc_index].cpu = -1;
		printf("Stopping cpu %d, process id %d.\n \n", cpu, procs[proc_index].pid);
		cpustop(cpu);
	}
	else if (strcmp(cmd,"createvm") == 0) {

		// search for an empty proc entry
		int proc_index;
		for (proc_index=0;proc_index < MAX_PROCS; proc_index++) {
			if (!procs[proc_index].pid) break;
		}
		if (proc_index == MAX_PROCS) {
			printf("Management program can not handle any more processes (max %d)\n", MAX_PROCS);
			return;
		}
		printf("\nLoading VM as a process \n");
		createvm(&procs[proc_index].pid);
		if (procs[proc_index].pid) {
		procs[proc_index].progname[0]='v';
		procs[proc_index].progname[1]='m';
		procs[proc_index].progname[2]='\0';
		procs[proc_index].cpu = -1;
			printf("VM  loaded as a process, pid %d\n", procs[proc_index].pid);
		}
		else {
			printf("Failed to load the vm\n");
		}
	}
	else if (strcmp(cmd,"setupvm") == 0) {
		setupvm();
	}
	else if (strcmp(cmd,"setuppios") == 0) {
		setuppios();
	}
	else if (strcmp(cmd,"shutdown") == 0) {
	}
	else if (strcmp(cmd,"help") == 0) {
		printf("The Management Module supports the following commands:\n");
		printf("list      - Prints out the programs available for loading\n");
		printf("load <num>- loads a program\n");
		printf("start <pid> <cpu>\n");
		printf("stop <cpu>\n");
		printf("status    - Prints out the status of the CPUs\n");
		printf("createvm  - create vmcb for a guest os\n");
		printf("setupvm  - notify the master kernel to start a vm guest\n");
		printf("setuppios  - notify the master kernel to start a vm guest of PIOS\n");
		printf("help      - Prints out this helpful information\n");
		printf("shutdown  - Terminates the kernel and shuts down the computer\n");
	}
	else {
		printf ("Unknown command \"%s\". For a list of commands type help\n", cmd);
	}
}

char buf[MAX_BUF];
signal sig;

void event(void) {
	uint32_t fcpu;
	uint32_t fpid;
	uint32_t faddr;
	uint32_t proc_index;
	//printf("event\n");
	switch (sig.type) {
		case SIGNAL_TIMER:
		//printf("Timer event received\n");
		break;

		case SIGNAL_PGFLT:
		fcpu = ((signal_pgflt*)&sig.data)->cpu;
		fpid = ((signal_pgflt*)&sig.data)->procid;
		faddr = ((signal_pgflt*)&sig.data)->fault_addr;
		proc_index = cpu_procs[fcpu];
		//printf("PGFLT on cpu %d, procid %d faulted at address %08x\n", fcpu, fpid, faddr);
		if (procs[proc_index].pid!=fpid)
		{
			printf("STOP cpu: %x\n",fcpu);
			cpustop(fcpu);
			break;
		}
	//	assert (procs[proc_index].pid == fpid);
		allocpage(fpid, faddr & ~(0xfff));
		//printf("Page allocated, restarting process\n");
		cpustart(fcpu,fpid);
		//printf("Restarted\n");
		break;

		default:
		printf("Unknown event\n");
	}
	cpu_signalret();
}


int main ()
{
	int i;
	printf("Untrusted Management Module\n");
	cpus = ncpu();
	cpu_signal(event, &sig);

	// clear out process table
	for(i=0;i<MAX_PROCS;i++)
		procs[i].pid = 0;

	for(i=0;i<255;i++)
		cpu_procs[i] = -1;


	while(1) {
		printf("Enter your command: ");
		waiting_for_input = 1;
		gets(buf, 512);
		waiting_for_input = 0;
		performCmd(buf);
	}
}
