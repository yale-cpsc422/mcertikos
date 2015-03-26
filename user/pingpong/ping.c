#include <proc.h>
#include <stdio.h>
#include <syscall.h>
#include <sysenter.h>

int
main (int argc, char **argv)
{
    unsigned int balls[] =
        { 1, 2, 3, 4, 5, 6, 7, 8, 9 };

    printf ("Ping sent 9 balls to pong.\n");
    unsigned int status = fast_sys_ssend (3, balls, 9);
    printf ("ping sent: %d\n", status);

    while (1)
    {
        fast_sys_yield ();
    }

//    if (status == E_IPC)
//        printf ("Bad thing happend in ping.\n");
//    else if (status == E_INVAL_PID)
//        printf ("Trying to send balls to a dead process.\n");

//printf("ping.\n");

    /*
     while(1) {
     //printf("ping yielding\n");
     yield();
     }
     */

    return 0;
}
