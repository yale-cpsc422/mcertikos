#include <preinit/lib/debug.h>
#include <preinit/lib/types.h>

#define NUM_CHAN		64
#define TD_STATE_RUN		1

extern uint8_t _binary___obj_user_idle_idle_start[];
extern uint8_t _binary___obj_user_pingpong_ping_start[];
extern uint8_t _binary___obj_user_pingpong_pong_start[];

extern void proc_init(unsigned int mbi_addr);
extern unsigned int proc_create(void *elf_addr);
extern unsigned int ring0proc_create(void);
extern void set_curid(unsigned int curid);
extern void tdq_remove(unsigned int chid, unsigned int pid);
extern void tcb_set_state(unsigned int pid, unsigned int state);
extern void kctx_switch(unsigned int from_pid, unsigned int to_pid);

static void
kern_main (void)
{
    unsigned int idle_pid;

    KERN_DEBUG("In kernel main.\n");
    idle_pid = proc_create (_binary___obj_user_pingpong_ping_start);
    proc_create (_binary___obj_user_pingpong_pong_start);
    KERN_DEBUG("process ping %d is created.\n", idle_pid);

    KERN_INFO("Start user-space ... \n");

    tdq_remove (NUM_CHAN, idle_pid);
    tcb_set_state (idle_pid, TD_STATE_RUN);
    set_curid (idle_pid);
    kctx_switch (0, idle_pid);

    KERN_PANIC("kern_main() should never be here.\n");
}

void
kern_init (uintptr_t mbi_addr)
{
    proc_init (mbi_addr);

    KERN_DEBUG("Kernel initialized.\n");

    kern_main ();
}
