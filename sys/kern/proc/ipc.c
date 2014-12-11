#include <preinit/lib/types.h>
#include <preinit/lib/debug.h>
#include <preinit/preinit.h>

#include "ipc_intro.h"
#include "sync_ipc_intro.h"

#define NUM_CHAN	64

extern unsigned int get_curid(void);
extern void thread_wakeup(unsigned int);
extern void thread_wakeup2(unsigned int);
extern void thread_sleep(unsigned int);
extern void thread_sleep2(void);
extern void sched_init(unsigned int);
//extern void vmcb_init(unsigned int);
//extern void vmx_init(unsigned int);

unsigned int
is_chan_ready(void)
{
	unsigned int chid, info;
	chid = get_curid();
	info = get_chan_info(chid);
	if (info == 0)
		return 0;
	else
		return 1;
}

unsigned int
send(unsigned int chid, unsigned int content)
{
	unsigned int info;

	if (0 <= chid && chid < NUM_CHAN) {
		info = get_chan_info(chid);

		if (info == 0) {
			set_chan_info(chid, 1);
			set_chan_content(chid, content);
			return 1;
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}

unsigned int
ssend(unsigned int chid, unsigned int content)
{
  unsigned int info;
  unsigned int myid = get_curid();

  KERN_ASSERT(0 <= myid && myid < NUM_CHAN);

  if (0 <= chid && chid < NUM_CHAN) {
    info = get_chan_info(chid); 
    // In synchronous IPC, no one
    // else should be able to write
    // to my designated channel except myself.
    KERN_ASSERT(info == 0);

    set_chan_info_for_proc(chid, myid, 1);
    set_chan_content_for_proc(chid, myid, content);
    thread_wakeup2(chid);
    thread_sleep2();
    return 1;
  } else {
    return 0;
  }
}

unsigned int
recv(void)
{
	unsigned int chid;
	unsigned int info;
	unsigned int content;

	chid = get_curid();
	info = get_chan_info(chid);

	if (info == 1) {
		content = get_chan_content(chid);
		set_chan_info(chid, 0);
		thread_wakeup(chid);
		return content;
	} else {
		return 0;
	}
}

unsigned int
srecv(unsigned int pid)
{
  unsigned int chid;
  unsigned int info;
  unsigned int content;

  chid = get_curid();
  info = get_chan_info_for_proc(chid, pid);

  if (info == 1) {
    content = get_chan_content_for_proc(chid, pid);
    set_chan_info_for_proc(chid, pid, 0);
    thread_wakeup2(pid);
    return content;
  } else {
    thread_sleep2();
    return srecv(pid);
  }
}

extern cpu_vendor cpuvendor;

void
proc_init(unsigned int mbi_addr)
{
	unsigned int i;
    
    set_vendor(); //sets the cpu vendor bit.

    /*
    if (cpuvendor == AMD) {
    	vmcb_init(mbi_addr);
    }
    else if(cpuvendor == INTEL) {
        vmx_init(mbi_addr);
    }
    */

  sched_init(mbi_addr);

	i = 0;
	while (i < NUM_CHAN) {
		init_chan(i, 0, 0);
    // Init synchronous channels
    unsigned int j;
    for (j = 0; j < NUM_CHAN; j++) {
      init_chan_for_proc(i, j, 0, 0);
    }
		i++;
	}
}
