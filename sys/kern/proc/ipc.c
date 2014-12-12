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
extern unsigned int tcb_get_state(unsigned int);
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
  unsigned int myid = get_curid();

  KERN_ASSERT(0 <= myid && myid < NUM_CHAN);

  if (0 <= chid && chid < NUM_CHAN) {

    append_node_to_list(chid, myid, content);

    unsigned int chidstate = tcb_get_state(chid);
    if (chidstate != 3) { // chid is not dead
      thread_wakeup2(chid);
    } else {
      return 2; // An error code to tell the user that it was contacting a dead proc.
    }
    thread_sleep2();
    return 1; // success
  } else {
    return 0; // bad chid
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

retry:
  chid = get_curid();
  info = is_node_in_list(chid, pid);

  if (info == 1) {
    content = get_node_data(pid);
    remove_node_from_list(chid, pid);
    unsigned int chidstate = tcb_get_state(pid);
    if (chidstate != 3) { // only wake it up if it's not dead
      thread_wakeup2(pid);
    }
    return content;
  } else {
    thread_sleep2();
    goto retry;
    return 0;
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
    init_ipc_node(i);
    init_ipc_list(i);
		i++;
	}
}
