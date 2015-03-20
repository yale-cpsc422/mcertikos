#include <preinit/dev/intr.h>
#include <lib/trap.h>
#include <preinit/lib/debug.h>
#include <preinit/lib/timing.h>

#include "exception.h"

/*
 * XXX: The 'redundant' return statements in spurious_intr_handler(),
 *      timer_inter_handler() and default_inter_handler() are used to prevent
 *      CompCert doing tail call opitimization.
 */

static int
spurious_intr_handler (void)
{
    return 0;
}

static int
timer_intr_handler (void)
{
    intr_eoi ();

    periodic();
    return 0;
}

static int
default_intr_handler (void)
{
    intr_eoi ();
    return 0;
}

void
interrupt_handler (void)
{
    unsigned int cur_pid;
    unsigned int trapno;

    cur_pid = get_curid ();
    trapno = uctx_get (cur_pid, U_TRAPNO);

    switch (trapno)
    {
    case T_IRQ0 + IRQ_SPURIOUS:
        spurious_intr_handler ();
        break;
    case T_IRQ0 + IRQ_TIMER:
        timer_intr_handler ();
        break;
    default:
        default_intr_handler ();
    }
}
