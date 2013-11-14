#include <virt/svm.h>

unsigned int
syscall_get_arg1(void)
{
	unsigned int cur_pid;
	unsigned int arg1;

	cur_pid = get_curid();
	arg1 = uctx_get(cur_pid, U_EAX);

	return arg1;
}

unsigned int
syscall_get_arg2(void)
{
	unsigned int cur_pid;
	unsigned int arg2;

	cur_pid = get_curid();
	arg2 = uctx_get(cur_pid, U_EBX);

	return arg2;
}

unsigned int
syscall_get_arg3(void)
{
	unsigned int cur_pid;
	unsigned int arg3;

	cur_pid = get_curid();
	arg3 = uctx_get(cur_pid, U_ECX);

	return arg3;
}

unsigned int
syscall_get_arg4(void)
{
	unsigned int cur_pid;
	unsigned int arg4;

	cur_pid = get_curid();
	arg4 = uctx_get(cur_pid, U_EDX);

	return arg4;
}

unsigned int
syscall_get_arg5(void)
{
	unsigned int cur_pid;
	unsigned int arg5;

	cur_pid = get_curid();
	arg5 = uctx_get(cur_pid, U_ESI);

	return arg5;
}

unsigned int
syscall_get_arg6(void)
{
	unsigned int cur_pid;
	unsigned int arg6;

	cur_pid = get_curid();
	arg6 = uctx_get(cur_pid, U_EDI);

	return arg6;
}

void
syscall_set_errno(unsigned int errno)
{
	unsigned int cur_pid;

	cur_pid = get_curid();
	uctx_set(cur_pid, U_EAX, errno);
}

void
syscall_set_retval1(unsigned int retval)
{
	unsigned int cur_pid;

	cur_pid = get_curid();
	uctx_set(cur_pid, U_EBX, retval);
}

void
syscall_set_retval2(unsigned int retval)
{
	unsigned int cur_pid;

	cur_pid = get_curid();
	uctx_set(cur_pid, U_ECX, retval);
}

void
syscall_set_retval3(unsigned int retval)
{
	unsigned int cur_pid;

	cur_pid = get_curid();
	uctx_set(cur_pid, U_EDX, retval);
}

void
syscall_set_retval4(unsigned int retval)
{
	unsigned int cur_pid;

	cur_pid = get_curid();
	uctx_set(cur_pid, U_ESI, retval);
}

void
syscall_set_retval5(unsigned int retval)
{
	unsigned int cur_pid;

	cur_pid = get_curid();
	uctx_set(cur_pid, U_EDI, retval);
}
