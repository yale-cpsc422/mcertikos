#include <kern/proc/proc.h>
#include <preinit/lib/debug.h>
#include <preinit/lib/types.h>
#include <lib/x86.h>
#include <preinit/lib/x86.h>
#include <preinit/lib/seg.h>
#include <kern/trap/sysenter.h>
#include <kern/proc/uctx.h>
#include <preinit/lib/timing.h>

#define PAGESIZE    4096
#define NUM_PROC    64
#define UCTX_SIZE   17

static void gcc_inline
trap_dump (ef_t *ef)
{
    if (ef == NULL)
        return;

    dprintf ("\t%08x:\tedi:   \t\t%08x\n", &ef->regs.edi, ef->regs.edi);
    dprintf ("\t%08x:\tesi:   \t\t%08x\n", &ef->regs.esi, ef->regs.esi);
    dprintf ("\t%08x:\tebp:   \t\t%08x\n", &ef->regs.ebp, ef->regs.ebp);
    dprintf ("\t%08x:\tesp:   \t\t%08x\n", &ef->regs.oesp, ef->regs.oesp);
    dprintf ("\t%08x:\tebx:   \t\t%08x\n", &ef->regs.ebx, ef->regs.ebx);
    dprintf ("\t%08x:\tedx:   \t\t%08x\n", &ef->regs.edx, ef->regs.edx);
    dprintf ("\t%08x:\tecx:   \t\t%08x\n", &ef->regs.ecx, ef->regs.ecx);
    dprintf ("\t%08x:\teax:   \t\t%08x\n", &ef->regs.eax, ef->regs.eax);
    /* dprintf("\t%08x:\tgs:    \t\t%08x\n", &tf->gs, tf->gs); */
    /* dprintf("\t%08x:\tfs:    \t\t%08x\n", &tf->fs, tf->fs); */
    dprintf ("\t%08x:\tes:    \t\t%08x\n", &ef->es, ef->es);
    dprintf ("\t%08x:\tds:    \t\t%08x\n", &ef->ds, ef->ds);
}

extern void _asm_sysenter_hdl (void);
extern void _asm_sysexit (ef_t * ef);
extern void tss_switch (uint32_t pid);
extern unsigned int ssend (unsigned int chid, uintptr_t vaddr, unsigned int scount);
extern unsigned int srecv (unsigned int pid, uintptr_t vaddr, unsigned int rcount);

void
sysenter_handler (ef_t * ef)
{
    tri(TR_YIELD, "yield enter");

    unsigned int sysnum = ef->regs.eax;
    unsigned int a1 = ef->regs.ebx;
    unsigned int a2 = ef->regs.esi;
    unsigned int a3 = ef->regs.edi;
    unsigned int * rv = &(ef->regs.eax);

    switch (sysnum)
    {
    case N_fsys_yield:
        thread_yield ();
        break;
    case N_fsys_ssend:
        set_pt (0);
        *rv = ssend (a1, a2, a3);
        break;
    case N_fsys_srecv:
        set_pt (0);
        *rv = srecv (a1, a2, a3);
        break;

    default:
        KERN_DEBUG("bad syscall %d.\n", sysnum);
    }

    tri(TR_YIELD, "yield exit");
    sysexit (ef);
}

void
sysexit (ef_t * ef)
{
    extern char STACK_LOC[NUM_PROC][PAGESIZE];

    unsigned int cur_pid = get_curid ();

    wrmsr(SYSENTER_ESP_MSR, (uint32_t) (&STACK_LOC[cur_pid][PAGESIZE-4]));
    if (get_pt () != cur_pid)
    {
        tss_switch(cur_pid);
        set_pt (cur_pid);
    }

//    KERN_DEBUG("exiting from %d\n", get_curid ());
    _asm_sysexit (ef);
}

void
sysenter_init ()
{
    wrmsr (SYSENTER_CS_MSR, CPU_GDT_KCODE);
    wrmsr (SYSENTER_EIP_MSR, (uint32_t) _asm_sysenter_hdl);
}
