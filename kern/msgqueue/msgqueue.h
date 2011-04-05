#ifndef _PIOS_KERN_MSGQUEUE
#define _PIOS_KERN_MSGQUEUE

void msgqueue_init(void);
bool msgqueue_add(char* buf, size_t size);
size_t msgqueue_get(char* buf, size_t size);
bool msgqueue_present(void);

#endif
