#include <proc.h>
#include <stdio.h>
#include <syscall.h>
#include <sysenter.h>

#define NUM_PROC 64

int
main (int argc, char **argv)
{
    printf ("idle\n");

#ifdef CONFIG_APP_USER_PROC

    pid_t ping_pid, pong_pid, ding_pid;

    if ((ping_pid = spawn (1)) != NUM_PROC)
        printf ("ping in process %d.\n", ping_pid);
    else
        printf ("Failed to launch ping.\n");

    if ((pong_pid = spawn (2)) != NUM_PROC)
        printf ("pong in process %d.\n", pong_pid);
    else
        printf ("Failed to launch pong.\n");

    if ((ding_pid = spawn (3)) != NUM_PROC)
        printf ("ding in process %d.\n", ding_pid);
    else
        printf ("Failed to launch ding.\n");
#endif

#ifdef CONFIG_APP_RING0_PROC
    pid_t ring0_id1, ring0_id2;
    if ((ring0_id1 = sys_ring0_spawn (1)) != NUM_PROC)
        printf ("The first ring0 process in process %d.\n", ring0_id1);
    else
        printf ("Failed to launch the first ring0 process.\n");

    if ((ring0_id2 = sys_ring0_spawn (2)) != NUM_PROC)
        printf ("The second ring0 process in process %d.\n", ring0_id2);
    else
        printf ("Failed to launch the second ring0 process.\n");
#endif

#ifdef CONFIG_APP_VMM
    pid_t vmm_pid;

    if ((vmm_pid = spawn (0)) != -1)
        printf ("VMM in process %d.\n", vmm_pid);
    else
        printf ("Failed to launch VMM.\n");
#endif

#ifdef CONFIG_APP_USER_PROFILE
    pid_t unit_pid;
    if ((unit_pid = spawn (4)) != NUM_PROC)
        printf ("unit in process %d.\n", unit_pid);
    else
        printf ("Failed to launch unit.\n");
#endif

    while (1)
        fast_sys_yield ();

    return 0;
}
