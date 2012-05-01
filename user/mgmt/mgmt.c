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
	CMD_SAFE_GETC,
	__CMD_DUMMY
} cmd_t;

typedef
enum {
	TYPE_INT, TYPE_STRING, TYPE_INVAL
} type_t;

#define CMD_LEN		12

#define ARGS_NUM	2
#define ARG_SIZE	32

struct cmd_table_t {
	cmd_t	cmd;
	char	cmd_string[CMD_LEN];

	int	nargs;
	type_t	arg_type[ARGS_NUM];
};

struct cmd_table_t cmd_table[__CMD_DUMMY] = {
	{CMD_CPUSTAT, "status", 0, {TYPE_INVAL, TYPE_INVAL}},
	{CMD_LOAD, "load", 2, {TYPE_INT, TYPE_INT}},
	{CMD_START, "start", 1, {TYPE_INT, TYPE_INVAL}},
	{CMD_STOP, "stop", 1, {TYPE_INT, TYPE_INVAL}},
	{CMD_STARTVM, "vm", 0, {TYPE_INVAL, TYPE_INVAL}},
	{CMD_SAFE_BITAND, "bitand", 2, {TYPE_INT, TYPE_INT}},
	{CMD_SAFE_BITOR, "bitor", 2, {TYPE_INT, TYPE_INT}},
	{CMD_SAFE_BITXOR, "bitxor", 2, {TYPE_INT, TYPE_INT}},
	{CMD_SAFE_BITNOT, "bitnot", 1, {TYPE_INT, TYPE_INVAL}},
	{CMD_SAFE_GETC, "getc", 0, {TYPE_INVAL, TYPE_INVAL}},
};

static struct {
	cmd_t	cmd;
	uint8_t	arg[ARGS_NUM][ARG_SIZE];
} parse_result;

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
	case CMD_START:
	case CMD_STOP:
		printf("Not implement yet.\n");
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

	memset(buf, 0x0, sizeof(char) * BUFSIZE);

	printf("Management shell starts.\n");

	while (1) {
		printf("# ");
		gets(buf, BUFSIZE);
		parse_cmd(buf);
		exec_cmd();
		memset(buf, 0, BUFSIZE * sizeof(char));
	}

	return 0;
}
