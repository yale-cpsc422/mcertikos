#include <preinit/lib/timing.h>
#include <preinit/lib/debug.h>
#include <preinit/lib/x86.h>
#include <preinit/dev/tsc.h>

unsigned long long jiffies = 0ull;

/**
 * Compute with 96 bit intermediate result: (a*b)/c
 */
static gcc_inline uint64_t
muldiv64 (uint64_t a, uint32_t b, uint32_t c)
{
    union
    {
        uint64_t ll;
        struct
        {
            uint32_t low, high;
        } l;
    } u, res;
    uint64_t rl, rh;

    u.ll = a;
    rl = (uint64_t) u.l.low * (uint64_t) b;
    rh = (uint64_t) u.l.high * (uint64_t) b;
    rh += (rl >> 32);
    res.l.high = rh / c;
    res.l.low = (((rh % c) << 32) + (rl & 0xffffffff)) / c;
    return res.ll;
}

int64_t gcc_inline
elapse_ns (uint64_t past, uint64_t now)
{
    KERN_ASSERT(now > 0 && past > 0);

    int64_t interval = now - past;

    return muldiv64 (interval, 1000u * 1000u, tsc_per_ms);
}

int64_t gcc_inline
elapse_us (uint64_t past, uint64_t now)
{
    KERN_ASSERT(now > 0 && past > 0);

    int64_t interval = now - past;

    return muldiv64 (interval, 1000u, tsc_per_ms);
}

int64_t gcc_inline
elapse_ms (uint64_t now, uint64_t past)
{
    KERN_ASSERT(now > 0 && past >= 0);

    int64_t interval = now - past;

    return interval / tsc_per_ms;
}

int32_t gcc_inline
elapse_s (uint64_t past, uint64_t now)
{
    KERN_ASSERT(now > 0 && past > 0);

    int64_t interval = (now - past) / 1000u;

    return interval / tsc_per_ms;
}

static uint64_t last_tsc = 0;

static uint32_t
action_print_dot (void * p)
{
    KERN_ASSERT(p == NULL);

    uint64_t now = rdtsc();
    vprintf ("%lld ", elapse_ms(now, last_tsc));

    last_tsc = now;

    return 0;

}

runnable_t periodic_run[] =
    { [0]=
        { .enable = TRUE, .period = IRQ_PERIOD(1), .param = NULL, .action =
                &action_print_dot, }, };

#define MAX_RUNNABLES sizeof(periodic_run) / sizeof(periodic_run[0])

void
periodic (void)
{
    int i;

    jiffies++;

    for (i = 0; i < MAX_RUNNABLES; i++)
    {
        if (periodic_run[i].enable && (jiffies % periodic_run[i].period == 0))
        {
            (*periodic_run[i].action) (periodic_run[i].param);
        }
    }
}

