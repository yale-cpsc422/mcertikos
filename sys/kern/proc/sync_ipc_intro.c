#include "sync_ipc_intro.h"

#define NUM_CHAN 64

struct SyncChanStruct {
  unsigned int isbusy;
  unsigned int content;
};

struct SyncChanStruct SYNCCHPOOL_LOC[NUM_CHAN][NUM_CHAN];

unsigned int
get_chan_info_for_proc(unsigned int chid, unsigned int pid)
{
  return SYNCCHPOOL_LOC[chid][pid].isbusy;
}

void
set_chan_info_for_proc(unsigned int chid, unsigned int pid,
                       unsigned int info)
{
  SYNCCHPOOL_LOC[chid][pid].isbusy = info;
}

unsigned int
get_chan_content_for_proc(unsigned int chid, unsigned int pid)
{
  return SYNCCHPOOL_LOC[chid][pid].content;
}

void
set_chan_content_for_proc(unsigned int chid, unsigned int pid,
                          unsigned int content)
{
  SYNCCHPOOL_LOC[chid][pid].content = content;
}

void
init_chan_for_proc(unsigned int chid, unsigned int pid,
                   unsigned int info, unsigned int content)
{
  SYNCCHPOOL_LOC[chid][pid].isbusy = info;
  SYNCCHPOOL_LOC[chid][pid].content = content;
}
