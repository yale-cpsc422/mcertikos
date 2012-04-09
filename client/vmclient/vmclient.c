#include <syscall.h>
#include <stdio.h>

int main() {
	printf("Vm is going to run...\n");
	start_vm_client();
	return 0;
}
