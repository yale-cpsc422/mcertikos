#include <proc.h>
#include <stdio.h>
#include <syscall.h>

#define WARMINGUP 10

inline void
unit_sys_get_tsc_per_ms ()
{
    sys_get_tsc_per_ms (0);
}

inline void
unit_sys_yield ()
{
    sys_yield ();
}

void
unit_profiling (uint64_t *t1, uint64_t *t2, void
(*fun) (), int trace_id)
{
    int i;
    for (i = 0; i < WARMINGUP; i++)
    {
        sys_start_trace (trace_id);
        *t1 = rdtsc ();
        (*fun) ();
        *t2 = rdtsc ();
        sys_stop_trace (trace_id);
    }

    sys_start_trace (trace_id);
    *t1 = rdtsc ();
    (*fun) ();
    *t2 = rdtsc ();
    sys_stop_trace (trace_id);
}

int
main (int argc, char **argv)
{
    printf ("profiling is running\n");

    uint64_t t1, t2;

    unit_profiling (&t1, &t2, unit_sys_get_tsc_per_ms, 1);

    printf ("sys_get_tsc_per_ms: start %llu, end %llu, (%llu)", t1, t2,
        t2 - t1);

    unit_profiling (&t1, &t2, unit_sys_yield, 0);

    printf ("sys_yield: start %llu, end %llu, (%llu)", t1, t2,
        t2 - t1);

    while (1)
    {
        yield ();
    }
    return 0;
}
