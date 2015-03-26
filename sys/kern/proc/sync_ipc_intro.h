#ifndef _KERN_VIRT_SYNC_IPC_INTRO_H_
#define _KERN_VIRT_SYNC_IPC_INTRO_H_

#ifdef _KERN_

#define MAX_BUFFSIZE 1000

void
init_ipc_node(unsigned int pid);

void
set_node_data(unsigned int pid,
              unsigned int vaddr,
              unsigned int count);

void
get_node_data(unsigned int pid,
              unsigned int *vaddr,
              unsigned int *count);

void
init_ipc_list(unsigned int listid);

void
append_node_to_list(unsigned int listid,
                    unsigned int pid);

// returns 0 on failure, 1 on success
unsigned int
remove_node_from_list(unsigned int listid,
                      unsigned int pid);

unsigned int
is_node_in_list(unsigned int listid,
                unsigned int pid);

#endif

#endif
