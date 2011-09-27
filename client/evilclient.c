#include <client/syscall.h>
#include <client/stdio.h>

int main() {

	long * target_address =0x100000;
	printf("Evil client, want to see the memory content at address: %x \n");
	printf("The value is\n");
	printf("%x",*target_address);
}
