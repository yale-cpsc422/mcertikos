#include <proc.h>
#include <stdio.h>
#include <syscall.h>
#include <sysenter.h>
#include <x86.h>

#define WARMUP 100
#define RECEIVE_BUF_SIZE 1000

#define FENCE() asm volatile ("" ::: "memory")


static gcc_inline int      
fast_sys_srecv_nop(uint32_t pid, unsigned int *buffer,
          uint32_t rcount)
{
    int rv;

    asm volatile(
            "movl %%esp, %%ecx;"
            "leal 1f, %%edx;"
            ".byte 0x66\n.byte 0x90;"
            "1:;"
            "movl %%eax, %0"
            : "=a" (rv)
            : "a" (N_fsys_srecv),
              "b" (pid),
              "S" (buffer),
              "D" (rcount)
            : "cc", "memory", "edx", "ecx"
    );

    return rv;
}

int
main (int argc, char **argv)
{
    unsigned int receivebuffer[RECEIVE_BUF_SIZE];
    unsigned int actualreceived = 0;
    unsigned long long start, end, overhead;
    int i;

    FENCE();
    for (i=0; i<WARMUP; i++) {
      start = rdtsc();
      actualreceived = fast_sys_srecv_nop(1, receivebuffer, RECEIVE_BUF_SIZE);
      end = rdtsc();
    }
    FENCE();
    overhead = end - start;

    FENCE();
    for (i=0; i<WARMUP; i++) {
      start = rdtsc();
      actualreceived = fast_sys_srecv(1, receivebuffer, RECEIVE_BUF_SIZE);
      end = rdtsc();
    }
    FENCE();

    printf("receive overhd: %llu\n", overhead);
    printf("(befor recv) %llu\n", start);
    printf("(after recv) %llu\n", end);

    printf("Pong received %u balls from ping.\n", actualreceived);

    while (1)
    {
        fast_sys_yield ();
    }

    return 0;
}
