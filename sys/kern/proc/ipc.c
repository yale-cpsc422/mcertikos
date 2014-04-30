#include <preinit/lib/types.h>
#include <preinit/lib/debug.h>
#include <preinit/preinit.h>

#include "ipc_intro.h"

#define NUM_CHAN	64

extern unsigned int get_curid(void);
extern void thread_wakeup(unsigned int);
extern void vmcb_init(unsigned int);
extern void vmx_init(unsigned int);

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

extern cpu_vendor cpuvendor;

void
proc_init(unsigned int mbi_addr)
{
	unsigned int i;
    
    set_vendor(); //sets the cpu vendor bit.

    if (cpuvendor == AMD) {
    	vmcb_init(mbi_addr);
    }
    else if(cpuvendor == INTEL) {
        vmx_init(mbi_addr);
    }

	i = 0;
	while (i < NUM_CHAN) {
		init_chan(i, 0, 0);
		i++;
	}
}
