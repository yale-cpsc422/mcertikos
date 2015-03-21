#include <preinit/lib/types.h>
#include <preinit/lib/debug.h>
#include <preinit/preinit.h>

#include "ipc_intro.h"
#include "sync_ipc_intro.h"

#define NUM_CHAN	   64
#define MAX_BUFFSIZE 32 // At most 32 integer at a time

#define PTE_P		0x001	/* Present */

#define PAGESIZE			4096

extern unsigned int get_curid(void);
extern void thread_wakeup(unsigned int);
extern void thread_wakeup2(unsigned int);
extern void thread_sleep(unsigned int);
extern void thread_sleep2(void);
extern void sched_init(unsigned int);
extern unsigned int tcb_get_state(unsigned int);
extern unsigned int pt_read(unsigned int pid, unsigned int vaddr);
#ifdef CONFIG_APP_VMM
extern void vmcb_init(unsigned int);
extern void vmx_init(unsigned int);
#endif

static inline
unsigned int *
getkernelpa (unsigned int chid, unsigned int kva)
{
    unsigned int kpa = pt_read (chid, kva);
    kpa = (kpa & 0xfffff000) + (kva % PAGESIZE);
    return (unsigned int *) kpa;
}

unsigned int
is_chan_ready (void)
{
    unsigned int chid, info;
    chid = get_curid ();
    info = get_chan_info (chid);
    if (info == 0)
        return 0;
    else
        return 1;
}

unsigned int
send (unsigned int chid, unsigned int content)
{
    unsigned int info;

    if (0 <= chid && chid < NUM_CHAN)
    {
        info = get_chan_info (chid);

        if (info == 0)
        {
            set_chan_info (chid, 1);
            set_chan_content (chid, content);
            return 1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }
}

/*
 * vaddr: Virtual address of buffer in the sender's address space
 * count: Number of items to send in buffer
 *
 * Note that it will only send MIN(MAX_BUFFSIZE, count) number of
 * items.
 *
 */
unsigned int
ssend (unsigned int chid, uintptr_t vaddr, unsigned int scount,
       uintptr_t actualsentva)
{
    unsigned int myid = get_curid ();

    KERN_ASSERT(0 <= myid && myid < NUM_CHAN);

    unsigned int chidstate = tcb_get_state (chid);

    // Return error code if trying
    // to send to dead process.
    if (chidstate == 3)
        return 2;

    if (0 <= chid && chid < NUM_CHAN)
    {
        // Set actual sent number
        unsigned int *asentpa = getkernelpa (myid, actualsentva);
        *asentpa = MIN(scount, MAX_BUFFSIZE);

        set_node_data (myid, vaddr, MIN(scount, MAX_BUFFSIZE));
        append_node_to_list (chid, myid);

        thread_wakeup2 (chid);
        thread_sleep2 ();

        return 1; // success
    }
    else
    {
        return 0; // bad chid
    }
}

unsigned int
recv (void)
{
    unsigned int chid;
    unsigned int info;
    unsigned int content;

    chid = get_curid ();
    info = get_chan_info (chid);

    if (info == 1)
    {
        content = get_chan_content (chid);
        set_chan_info (chid, 0);
        thread_wakeup (chid);
        return content;
    }
    else
    {
        return 0;
    }
}

/*
 * vaddr: Virtual address of recveiving buffer in the receiver's
 * address space.
 * count: Max number of items to receive
 *
 * Note that the actual number received would be the MIN(scount,
 * rcount, MAX_BUFFSIZE);
 *
 */
unsigned int
srecv (unsigned int pid, uintptr_t vaddr, unsigned int rcount,
       uintptr_t actualreceivedva)
{
    unsigned int chid;
    unsigned int info;
    unsigned int chidstate;

    retry: chid = get_curid ();
    info = is_node_in_list (chid, pid);
    chidstate = tcb_get_state (pid);

    // Return error code if trying
    // to receive from dead process.
    if (chidstate == 3)
        return 2;

    unsigned int senderva;
    unsigned int scount;

    if (info == 1)
    {
        get_node_data (pid, &senderva, &scount);

        unsigned int *arecvpa = getkernelpa (chid, actualreceivedva);
        *arecvpa = MIN(rcount, scount);

        unsigned int i;
        unsigned int *rbuff = getkernelpa (chid, vaddr);
        unsigned int *sbuff = getkernelpa (pid, senderva);
        for (i = 0; i < *arecvpa; i++)
        {
            rbuff[i] = sbuff[i];
        }

        remove_node_from_list (chid, pid);

        thread_wakeup2 (pid);
        return 1;
    }
    else
    {
        thread_sleep2 ();
        goto retry;
        return 0; // This should not be reachable
    }
}

extern cpu_vendor cpuvendor;

void
proc_init (unsigned int mbi_addr)
{
    unsigned int i;

    set_vendor (); //sets the cpu vendor bit.

#ifdef CONFIG_APP_VMM
    if (cpuvendor == AMD)
    {
        vmcb_init (mbi_addr);
    }
    else if (cpuvendor == INTEL)
    {
        vmx_init (mbi_addr);
    }
#endif

#if defined(CONFIG_APP_USER_PROC) || defined (CONFIG_APP_RING0_PROC)
    sched_init (mbi_addr);
#endif

    i = 0;
    while (i < NUM_CHAN)
    {
        init_chan (i, 0, 0);
        // Init synchronous channels
        init_ipc_node (i);
        init_ipc_list (i);
        i++;
    }
}
