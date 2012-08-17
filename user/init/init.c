#include <proc.h>
#include <stdio.h>
#include <types.h>

extern uint8_t _binary___obj_user_idle_idle_start[];
extern uint8_t _binary___obj_user_guest_guest_start[];

extern uint8_t _binary___obj_user_msg_sender_start[];
extern uint8_t _binary___obj_user_msg_receiver_start[];

int
main(int argc, char **argv)
{
#if 0
	pid_t sender, receiver, idle;

	idle = spawn((uintptr_t) _binary___obj_user_idle_idle_start);
	printf("idle process (pid %d) is created.\n", idle);

	sender = spawn((uintptr_t) _binary___obj_user_msg_sender_start);
	printf("sender (pid %d) is created.\n", sender);

	receiver = spawn((uintptr_t) _binary___obj_user_msg_receiver_start);
	printf("receiver (pid %d) is created.\n", receiver);

	send(sender, &receiver, sizeof(receiver));

#else
	pid_t guest = spawn((uintptr_t) _binary___obj_user_guest_guest_start);
	printf("guest (pid %d) is created.\n", guest);
#endif

	return 0;
}
