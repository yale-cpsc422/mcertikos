#include <inc/arch/types.h>
#include <user/stdio.h>


// atoi - parse a decimal integer from a string
// takes a string buffer, and a pointer to an integer where to deposit the result
// returns number of characters used for an integer, or 0 if no number present
int atoi(const char* buf, int* i) {

	int loc=0;
	int numstart=0;
	uint32_t acc=0;
	bool negative = false;
	if (buf[loc] == '+')
		loc++;
	else if (buf[loc] == '-') {
		negative = true;
		loc++;
	}
	numstart = loc;
	// no grab the numbers
	while ('0' <= buf[loc] && buf[loc] <= '9') {
		acc = acc*10 + (buf[loc]-'0');
		loc++;
	}
	if (numstart == loc) {
		// no numbers have actually been scanned
		return 0;
	}
	if (negative)
		acc = - acc;
	*i = acc;
	return loc;
}
