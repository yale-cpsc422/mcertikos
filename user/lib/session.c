#include <session.h>
#include <syscall.h>
#include <types.h>

sid_t
session(session_type type)
{
	return sys_session(type);
}

sid_t
getsid(void)
{
	return sys_getsid();
}
