#include <proc.h>
#include <stdio.h>
#include <syscall.h>
#include <sysenter.h>

int
main (int argc, char **argv)
{

    unsigned int receivebuffer[32];

    unsigned int status = fast_sys_srecv (2, receivebuffer, 32);

    printf ("size 2: %d\n", status);

//    if (status == E_IPC)
//        printf ("Bad thing happened in pong.\n");
//    else if (status == E_INVAL_PID)
//        printf ("Trying to receive from dead process.\n");

//    printf ("Status: %d\n", status);

    status = fast_sys_srecv (4, receivebuffer, 32);
    printf ("size 4: %d\n", status);

    while (1)
    {
        fast_sys_yield ();
    }

//    if (status == E_IPC)
//        printf ("Bad thing happened in pong.\n");
//    else if (status == E_INVAL_PID)
//        printf ("Trying to receive from dead process.\n");

    //printf("pong.\n");

    /*
     while(1) {
     //printf("pong yielding\n");
     yield();
     }
     */

    return 0;
}
