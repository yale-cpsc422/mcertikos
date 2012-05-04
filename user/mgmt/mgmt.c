#include <hypercall_svm.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

#define BUFSIZE		512

typedef
enum {
	CMD_CPUSTAT, CMD_LOAD, CMD_START, CMD_STOP, CMD_STARTVM,
	CMD_SAFE_BITAND, CMD_SAFE_BITOR, CMD_SAFE_BITXOR, CMD_SAFE_BITNOT,
	CMD_SAFE_GETC, CMD_LIST,
	__CMD_DUMMY
} cmd_t;

typedef
enum {
	TYPE_INT, TYPE_STRING, TYPE_INVAL
} type_t;

#define CMD_LEN		12

#define ARGS_NUM	2
#define ARG_SIZE	32

#define MAX_PROG 10
#define MAX_PROGNAME 50
#define MAX_PROCS 50

int cpus;
//bounded client pplications named by their paths
extern char _binary___obj_client_client1_client1_start[];
extern char _binary___obj_client_vmclient_vmclient_start[];
extern char _binary___obj_client_evilclient_evilclient_start[];

struct program {
        char progname[MAX_PROGNAME];
        char * prog_ptr;
};

struct program programs[] =
{
        {"client1", _binary___obj_client_client1_client1_start},
        {"evilclient", _binary___obj_client_evilclient_evilclient_start},
        {"vmclient", _binary___obj_client_vmclient_vmclient_start},
        {"", (char *)0}
};

static int prog_num = 3;
//static int waiting_for_input;

struct proc_info {
        uint32_t pid; // 0 = empty  
        char progname[MAX_PROGNAME];
        int cpu; // -1 = not active};
};

static struct proc_info procs[MAX_PROCS];
static int cpu_procs[255]; // stores the proc_index of the process running on that CPU


struct cmd_table_t {
	cmd_t	cmd;
	char	cmd_string[CMD_LEN];

	int	nargs;
	type_t	arg_type[ARGS_NUM];
};

struct cmd_table_t cmd_table[__CMD_DUMMY] = {
	{CMD_CPUSTAT, "status", 0, {TYPE_INVAL, TYPE_INVAL}},
	{CMD_LOAD, "load", 1, { TYPE_INT}},
	{CMD_START, "start", 2, {TYPE_INT, TYPE_INT}},
	{CMD_STOP, "stop", 1, {TYPE_INT, TYPE_INVAL}},
	{CMD_STARTVM, "vm", 0, {TYPE_INVAL, TYPE_INVAL}},
	{CMD_SAFE_BITAND, "bitand", 2, {TYPE_INT, TYPE_INT}},
	{CMD_SAFE_BITOR, "bitor", 2, {TYPE_INT, TYPE_INT}},
	{CMD_SAFE_BITXOR, "bitxor", 2, {TYPE_INT, TYPE_INT}},
	{CMD_SAFE_BITNOT, "bitnot", 1, {TYPE_INT, TYPE_INVAL}},
	{CMD_SAFE_GETC, "getc", 0, {TYPE_INVAL, TYPE_INVAL}},
	{CMD_LIST, "list", 0, {TYPE_INVAL, TYPE_INVAL}},
};

static struct {
	cmd_t	cmd;
	uint8_t	arg[ARGS_NUM][ARG_SIZE];
} parse_result;

static void
init_cmd_table()
{
	int i, j;

	for (i = 0; i < __CMD_DUMMY; i++)
		for (j = 0; j < ARGS_NUM; j++)
			cmd_table[i].arg_type[j] = TYPE_INVAL;

	cmd_table[CMD_CPUSTAT].cmd = CMD_CPUSTAT;
	strncpy(cmd_table[CMD_CPUSTAT].cmd_string, "status", CMD_LEN);
	cmd_table[CMD_CPUSTAT].nargs = 0;

	cmd_table[CMD_LOAD].cmd = CMD_LOAD;
	strncpy(cmd_table[CMD_LOAD].cmd_string, "load", CMD_LEN);
	cmd_table[CMD_LOAD].nargs = 1;
	cmd_table[CMD_LOAD].arg_type[0] = TYPE_INT;
//	cmd_table[CMD_LOAD].arg_type[1] = TYPE_INT;

	cmd_table[CMD_START].cmd = CMD_START;
	strncpy(cmd_table[CMD_START].cmd_string, "start", CMD_LEN);
	cmd_table[CMD_START].nargs = 2;
	cmd_table[CMD_START].arg_type[0] = TYPE_INT;
	cmd_table[CMD_START].arg_type[1] = TYPE_INT;

	cmd_table[CMD_STOP].cmd = CMD_STOP;
	strncpy(cmd_table[CMD_STOP].cmd_string, "stop", CMD_LEN);
	cmd_table[CMD_STOP].nargs = 1;
	cmd_table[CMD_STOP].arg_type[0] = TYPE_INT;

	cmd_table[CMD_STARTVM].cmd = CMD_STARTVM;
	strncpy(cmd_table[CMD_STARTVM].cmd_string, "vm", CMD_LEN);
	cmd_table[CMD_STARTVM].nargs = 0;
}

static const char *
skip_blanks(const char *buf, const char *end)
{
	const char *ptr = buf;

	while (ptr != end && *ptr != '\0' &&(*ptr == ' ' || *ptr == '\t'))
		ptr++;

	return ptr;
}

static const char *
skip_nonblanks(const char *buf, const char *end)
{
	const char *ptr = buf;

	while (ptr != end && ptr != '\0' && *ptr != ' ' && *ptr != '\t')
		ptr++;

	return ptr;
}

static const char *
goto_next_field(const char *buf, const char *end)
{
	const char *ptr = buf;

	ptr = skip_blanks(ptr, end);
	if (ptr == end || *ptr == '\0')
		return NULL;

	ptr = skip_nonblanks(ptr, end);
	if (ptr == end || *ptr == '\0')
		return NULL;

	ptr = skip_blanks(ptr, end);
	if (ptr == end || *ptr == '\0')
		return NULL;

	return ptr;
}

static void
parse_cmd(const char *buf)
{
	int i;
	const char *ptr = buf;
	struct cmd_table_t *cmd;

	/* parse the command */
	ptr = skip_blanks(ptr, &buf[BUFSIZE]);

	if (ptr == NULL || ptr[0] == '\n')
		goto parse_err;

	for (i = 0; i < __CMD_DUMMY; i++) {
		cmd = &cmd_table[i];
		if (strncmp(ptr, cmd->cmd_string, strlen(cmd->cmd_string)) == 0)
			break;
	}

	if (i == __CMD_DUMMY) {
		printf("Unrecognized command.\n");
		goto parse_err;
	}

	parse_result.cmd = cmd->cmd;

	/* parse arguments */
	for (i = 0; i < cmd->nargs; i++) {
		ptr = goto_next_field(ptr, &buf[BUFSIZE]);
		if (ptr == NULL) {
			printf("Too few arguments.\n");
			goto parse_err;
		}

		switch (cmd->arg_type[i]) {
		case TYPE_INT:
			atoi(ptr, (int *) parse_result.arg[i]);
			break;

		case TYPE_STRING:
			;
			char *arg_ptr = (char *) parse_result.arg[i];
			const char *arg_end =
				(char *) &parse_result.arg[i][ARG_SIZE-1];
			while (arg_ptr != arg_end &&
			       *ptr != ' ' && *ptr != '\t' && *ptr != '\0') {
				*arg_ptr = *ptr;
				arg_ptr++;
				ptr++;
			}
			*arg_ptr = '\0';
			break;

		default:
			printf("Unrecognized argument.\n");
			goto parse_err;
		}

	}

	ptr = goto_next_field(ptr, &buf[BUFSIZE]);
	if (ptr != NULL) {
		printf("buf @ %x, ptr @ %x\n", buf, ptr);
		printf("Too many arguments.\n");
		goto parse_err;
	}

	return;

 parse_err:
	parse_result.cmd = __CMD_DUMMY;
	return;
}

static void
exec_cmd()
{
	switch (parse_result.cmd) {
	case __CMD_DUMMY:
		break;

	case CMD_CPUSTAT:
		printf("CPU Status: ");

		int i;
		int ncpu = sys_ncpu();

		for (i = 0; i < ncpu; i++)
			printf("%d ", sys_cpustat(i));

		printf("\n");

		break;

	case CMD_STARTVM:
		printf("Start VM ... \n");
		sys_startupvm();
		break;

	case CMD_LOAD:
		printf("Loading ... \n");
		int program_idx;
		program_idx=  * (int *)parse_result.arg[0];
		printf("arg0:%d,arg1:%d",*(int * )parse_result.arg[0],*(int * )parse_result.arg[1]);
		printf("program_idx:%d",program_idx);
		if (!(0<=program_idx && program_idx < prog_num)) {
                   printf("Program number %d does not exist\n", program_idx);
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

         	printf("Loading program %s\n", programs[program_idx].progname);
                
		sys_load((uintptr_t)&programs[program_idx].prog_ptr, &procs[proc_index].pid);

                if (procs[proc_index].pid) {
                        strncpy(procs[proc_index].progname, programs[program_idx].progname, MAX_PROGNAME);
                        procs[proc_index].cpu = -1;
                        printf("Program loaded, pid %d\n", procs[proc_index].pid);
                }
                else {
                        printf("Failed to load the program\n");
                }
	
		break;
	case CMD_START:
		printf("Starting ... \n");
		int pid=0;
		int cpu=0;

		cpu= *(int * ) parse_result.arg[0];
		pid= *(int * ) parse_result.arg[1];
	
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
		break;
	case CMD_STOP:
		printf("Stopping ...\n");

		cpu= *(int * ) parse_result.arg[0];
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
                stop_cpu(cpu);

		break;

	case CMD_LIST:
		i=0;
                printf("The mgmt program can load the following programs:\n");
                while(programs[i].prog_ptr) {
                        printf("%02d. %s\n", i, programs[i].progname);
                        i++;
                }
		break;

	case CMD_SAFE_BITAND: {
		uint32_t a0 = *(uint32_t *) parse_result.arg[0];
		uint32_t a1 = *(uint32_t *) parse_result.arg[1];
		uint32_t b = hypercall_bitand(a0, a1);
		printf("0x%08x & 0x%08x => 0x%08x\n", a0, a1, b);
		break;
	}

	case CMD_SAFE_BITOR: {
		uint32_t a0 = *(uint32_t *) parse_result.arg[0];
		uint32_t a1 = *(uint32_t *) parse_result.arg[1];
		uint32_t b = hypercall_bitor(a0, a1);
		printf("0x%08x | 0x%08x => 0x%08x\n", a0, a1, b);
		break;
	}

	case CMD_SAFE_BITXOR: {
		uint32_t a0 = *(uint32_t *) parse_result.arg[0];
		uint32_t a1 = *(uint32_t *) parse_result.arg[1];
		uint32_t b = hypercall_bitxor(a0, a1);
		printf("0x%08x ^ 0x%08x => 0x%08x\n", a0, a1, b);
		break;
	}

	case CMD_SAFE_BITNOT: {
		uint32_t a0 = *(uint32_t *) parse_result.arg[0];
		uint32_t b = hypercall_bitnot(a0);
		printf("~0x%08x => 0x%08x\n", a0, b);
		break;
	}

	case CMD_SAFE_GETC: {
		char c = hypercall_getc();
		printf("[echo] %x\n", c);
		break;
	}

	default:
		break;
	}
}

int main()
{
	char buf[BUFSIZE];
	int i;
	
	cpus=sys_ncpu();
        // clear out process table
        for(i=0;i<MAX_PROCS;i++)
                procs[i].pid = 0;
              
        for(i=0;i<255;i++)
                cpu_procs[i] = -1;


	memset(buf, 0x0, sizeof(char) * BUFSIZE);

	printf("Management shell starts.\n");
	
//	init_cmd_table();

	while (1) {
		printf("# ");
		gets(buf, BUFSIZE);
		parse_cmd(buf);
		exec_cmd();
		memset(buf, 0, BUFSIZE * sizeof(char));
	}

	return 0;
}
