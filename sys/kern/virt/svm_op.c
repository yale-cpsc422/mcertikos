#include "svm_switch.h"

extern void host_out(void);

void
switch_to_guest(void)
{
	host_out();
}

void
switch_to_host(void)
{
}
