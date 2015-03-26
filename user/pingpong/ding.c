#include <proc.h>
#include <stdio.h>
#include <syscall.h>
#include <sysenter.h>

int
main (int argc, char **argv)
{
    unsigned int balls[] =
        { 0, 1, 2 };
    unsigned int scount = 3;
    printf ("Ding sent 3 balls to pong.\n");
    unsigned int status = fast_sys_ssend (3, balls, scount);
    printf ("ding sent: %d\n", status);

    while (1)
    {
        fast_sys_yield ();
    }

//    if (status == E_IPC)
//        printf ("Bad thing happend in ding.\n");
//    else if (status == E_INVAL_PID)
//        printf ("Trying to send balls to a dead process.\n");

//printf("DING\n");
    return 0;
}
