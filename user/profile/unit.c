#include <proc.h>
#include <stdio.h>
#include <syscall.h>
#include <sysenter.h>

#define WARMINGUP 10
#define NO_WARMINGUP 0

inline void
unit_sys_get_tsc_per_ms ()
{
    sys_get_tsc_per_ms (0);
}

inline void
unit_sys_yield ()
{
    //sys_yield ();
    fast_sys_yield ();
}

inline void
unit_sys_spawn ()
{
    spawn (5);
}

// 16 pages
unsigned char chunk[(WARMINGUP + 2) * 0x1000] __attribute__((aligned(0x1000)));
static int current_page_idx = 0;

inline void
unit_pft_handling ()
{
    *(chunk + 0x1000 * current_page_idx + 1) = 1u;
    current_page_idx++;
}

void
unit_profiling (uint64_t *t1, uint64_t *t2, void
(*fun) (),
                int trace_id, int warmup)
{
    int i;
    for (i = 0; i < warmup; i++)
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

    unit_profiling (&t1, &t2, unit_sys_yield, 0, WARMINGUP);

    printf ("sys_yield:\n start %llu\n end %llu\n (%llu)", t1, t2, t2 - t1);

//    unit_profiling (&t1, &t2, unit_sys_get_tsc_per_ms, 1, WARMINGUP);
//
//    printf ("sys_get_tsc_per_ms: start %llu\n end %llu\n (%llu)", t1, t2,
//        t2 - t1);
//
//    unit_profiling (&t1, &t2, unit_sys_spawn, 2, WARMINGUP);
//
//    printf ("sys_spawn: start %llu\n end %llu\n (%llu)", t1, t2, t2 - t1);
//
//    printf ("chunk = 0x%08x\n", chunk);
//    current_page_idx = 0;
//
//    unit_profiling (&t1, &t2, unit_pft_handling, 3, WARMINGUP);
//
//    printf ("pgf_handl: start %llu\n end %llu\n (%llu)", t1, t2, t2 - t1);

    while (1)
    {
        yield ();
    }
    return 0;
}
