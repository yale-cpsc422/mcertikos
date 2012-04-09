#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

#define BUFSIZE		512

typedef
enum {
	CMD_CPUSTAT, CMD_LOAD, CMD_START, CMD_STOP, CMD_STARTVM, __CMD_DUMMY
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

//bounded client pplications named by their paths
extern uint8_t _binary___obj_client_client1_client1_start[];
extern uint8_t _binary___obj_client_vmclient_vmclient_start[];
extern uint8_t _binary___obj_client_evilclient_evilclient_start[];

struct program {
        char progname[MAX_PROGNAME];
        uint8_t * prog_ptr;
};

struct program programs[] =
{
        {"client1", _binary___obj_client_client1_client1_start},
        {"vmclient", _binary___obj_client_vmclient_vmclient_start},
        {"evilclient", _binary___obj_client_evilclient_evilclient_start},
        {"", (uint8_t *)0}
};

static int prog_num = 3;
//static int waiting_for_input;

struct proc_info {
        uint32_t pid; // 0 = empty  
        char progname[MAX_PROGNAME];
        int cpu; // -1 = not active};
};

static struct proc_info procs[MAX_PROCS];
//static int cpu_procs[255]; // stores the proc_index of the process running on that CPU


struct cmd_table_t {
	cmd_t	cmd;
	char	cmd_string[CMD_LEN];

	int	nargs;
	type_t	arg_type[ARGS_NUM];
} cmd_table[__CMD_DUMMY];

struct {
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
	cmd_table[CMD_LOAD].nargs = 2;
	cmd_table[CMD_LOAD].arg_type[0] = TYPE_INT;
	cmd_table[CMD_LOAD].arg_type[1] = TYPE_INT;

	cmd_table[CMD_START].cmd = CMD_START;
	strncpy(cmd_table[CMD_START].cmd_string, "start", CMD_LEN);
	cmd_table[CMD_START].nargs = 1;
	cmd_table[CMD_START].arg_type[0] = TYPE_INT;

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

	if (ptr == NULL)
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
                
		sys_load((uintptr_t )programs[program_idx].prog_ptr, &procs[proc_index].pid);

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
		break;
	case CMD_STOP:
		printf("Stopping ...\n");
		break;

	default:
		break;
	}
}

int main()
{
	char buf[BUFSIZE];

	memset(buf, 0x0, sizeof(char) * BUFSIZE);

	printf("Management shell starts.\n");

	init_cmd_table();

	while (1) {
		printf("# ");
		gets(buf, BUFSIZE);
		parse_cmd(buf);
		exec_cmd();
		memset(buf, 0, BUFSIZE * sizeof(char));
	}

	return 0;
}
