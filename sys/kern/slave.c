#include <sys/debug.h>
#include <sys/slave.h>
#include <sys/types.h>
#include <sys/x86.h>

void
slave_kernel(void)
{
	halt();
}
