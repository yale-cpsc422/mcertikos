#include "sync_ipc_intro.h"
#include <preinit/lib/debug.h>

#define NUM_CHAN 64

struct SyncIPCStruct {
  unsigned int data;
  unsigned int pid;
  unsigned int prev;
  unsigned int next;
  unsigned int listid;
};

struct SyncIPCStruct IPCNode_Pool[NUM_CHAN];
unsigned int         IPCLists[NUM_CHAN];

void
init_ipc_node(unsigned int pid)
{
  IPCNode_Pool[pid].data = 0;
  IPCNode_Pool[pid].pid  = pid;
  IPCNode_Pool[pid].next = NUM_CHAN;
  IPCNode_Pool[pid].prev = NUM_CHAN;
  IPCNode_Pool[pid].listid = NUM_CHAN;
}

void
set_node_listid(unsigned int pid, unsigned int listid)
{
  IPCNode_Pool[pid].listid = listid;
}

unsigned int
get_node_listid(unsigned int pid)
{
  return IPCNode_Pool[pid].listid;
}

void
set_node_data(unsigned int pid, unsigned int data)
{
  IPCNode_Pool[pid].data = data;
}

unsigned int
get_node_data(unsigned int pid)
{
  return IPCNode_Pool[pid].data;
}

void
set_node_next(unsigned int pid, unsigned int next)
{
  IPCNode_Pool[pid].next = next;
}

unsigned int
get_node_next(unsigned int pid)
{
  return IPCNode_Pool[pid].next;
}

void
set_node_prev(unsigned int pid, unsigned int prev)
{
  IPCNode_Pool[pid].prev = prev;
}

unsigned int
get_node_prev(unsigned int pid)
{
  return IPCNode_Pool[pid].prev;
}

void
init_ipc_list(unsigned int listid)
{
  IPCLists[listid] = NUM_CHAN;
}

unsigned int
is_list_empty(unsigned int listid)
{
  return IPCLists[listid] == NUM_CHAN;
}

void
append_node_to_list(unsigned int listid, unsigned int pid, unsigned int data)
{
  unsigned int isempty = is_list_empty(listid);

  set_node_data(pid, data);
  set_node_listid(pid, listid);

  if (isempty) {
    IPCLists[listid] = pid;
    set_node_next(pid, NUM_CHAN);
    set_node_prev(pid, NUM_CHAN);
  } else {
    unsigned int curridx = IPCLists[listid]; 
    while (NUM_CHAN != get_node_next(curridx)) {
      curridx = get_node_next(curridx);
    }
    set_node_next(curridx, pid);
    set_node_prev(pid, curridx);
    set_node_next(pid, NUM_CHAN);
  }
}

unsigned int
remove_head_from_list(unsigned int listid)
{
  if (is_list_empty(listid)) {
    return 0;
  } else {
    unsigned int head = IPCLists[listid];
    unsigned int next = get_node_next(head);

    KERN_ASSERT(get_node_listid(head) == listid);

    set_node_next(head, NUM_CHAN);
    set_node_prev(head, NUM_CHAN);
    set_node_listid(head, NUM_CHAN);

    if (next == NUM_CHAN) {
      IPCLists[listid] = NUM_CHAN;
    } else {
      IPCLists[listid] = next;
      set_node_prev(next, NUM_CHAN);
    }

    return 1;
  }
}

unsigned int
remove_node_from_list(unsigned int listid, unsigned int pid)
{
  if (is_list_empty(listid)) {

    return 0;

  } else {

    if (get_node_listid(pid) != listid) {
      return 0;
    }

    unsigned int head = IPCLists[listid];
    unsigned int next = get_node_next(pid);
    unsigned int prev = get_node_prev(pid);

    unsigned int success;

    if (pid == head) { // head

      success = remove_head_from_list(listid);

    } else if (next != NUM_CHAN) { // middle

      set_node_next(prev, next);
      set_node_prev(next, prev);

      success = 1;

    } else { // tail

      set_node_next(prev, NUM_CHAN); 

      success = 1;
    }

    /* 1, 2, 3 empties out the node */
    set_node_next(pid, NUM_CHAN);   // 1
    set_node_prev(pid, NUM_CHAN);   // 2
    set_node_listid(pid, NUM_CHAN); // 3

    return success;
  }
}

unsigned int
is_node_in_list(unsigned int listid, unsigned int pid)
{
  return IPCNode_Pool[pid].listid == listid;
}
