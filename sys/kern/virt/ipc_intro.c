#include "ipc_intro.h"
#include "vmcb_op.h"

#define NUM_CHAN	64

struct ChanStruct {
	unsigned int isbusy;
	unsigned int content;
};

struct ChanStruct CHPOOL_LOC[NUM_CHAN];

unsigned int
get_chan_info(unsigned int chid)
{
	return CHPOOL_LOC[chid].isbusy;
}

void
set_chan_info(unsigned int chid, unsigned int info)
{
	CHPOOL_LOC[chid].isbusy = info;
}

unsigned int
get_chan_content(unsigned int chid)
{
	return CHPOOL_LOC[chid].content;
}

void
set_chan_content(unsigned int chid, unsigned int content)
{
	CHPOOL_LOC[chid].content = content;
}

unsigned int
init_chan(unsigned int chid, unsigned int info, unsigned int content)
{
	CHPOOL_LOC[chid].isbusy = info;
	CHPOOL_LOC[chid].content = content;
}
