#include <proc.h>
#include <stdio.h>
#include <syscall.h>
#include <sysenter.h>
#include <x86.h>

#define WARMUP 100
#define SEND_BUF_SIZE 1000

#define FENCE() asm volatile ("" ::: "memory")

static gcc_inline int
fast_sys_ssend_nop(uint32_t chid, unsigned int *buffer,
          uint32_t scount)
{
    int rv;

    asm volatile(
            "movl %%esp, %%ecx;"
            "leal 1f, %%edx;"
            ".byte 0x66\n.byte 0x90;"
            "1:;"
            "movl %%eax, %0"
            : "=a" (rv)
            : "a" (N_fsys_ssend),
              "b" (chid),
              "S" (buffer),
              "D" (scount)
            : "cc", "memory", "edx", "ecx"
    );

    return rv;
}

int
main (int argc, char **argv)
{
    unsigned int balls[SEND_BUF_SIZE];
    unsigned int actualsent;
    unsigned long long start, end, overhead;
    int i;

    FENCE();
    for (i=0; i<WARMUP; i++) {
      start = rdtsc();
      actualsent = fast_sys_ssend_nop(2, balls, SEND_BUF_SIZE);
      end = rdtsc();
    }
    FENCE();
    overhead = end - start;

    FENCE();
    for (i=0; i<WARMUP; i++) {
      start = rdtsc();
      actualsent = fast_sys_ssend(2, balls, SEND_BUF_SIZE);
      end = rdtsc();
    }
    FENCE();

    printf("send overhd: %llu\n", overhead);
    printf("(befor send) %llu\n", start);
    printf("(after send) %llu\n", end);

    printf("Ping actually sent %d balls.\n", actualsent);

    while (1)
    {
        fast_sys_yield ();
    }

    return 0;
}
