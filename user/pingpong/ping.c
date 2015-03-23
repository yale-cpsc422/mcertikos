#include <proc.h>
#include <stdio.h>
#include <syscall.h>

int
main(int argc, char **argv)
{
	uint64_t t1, t2;
	int i;
	for (i = 0; i < 5; i ++)
	{
        sys_start_trace (0);
        t1 = rdtsc ();
        sys_get_tsc_per_ms(0);
        t2 = rdtsc ();
        sys_stop_trace (0);
	}

    sys_start_trace (0);
    t1 = rdtsc ();
    sys_get_tsc_per_ms(0);
    t2 = rdtsc ();
    sys_stop_trace (0);
    printf ("t1 = %llu, t2 = %llu\n", t1, t2);

    end:
    goto end;

    printf("should never be here!");

	return 0;
}
