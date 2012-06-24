#include <proc.h>
#include <stdio.h>
#include <types.h>

extern uint8_t _binary___obj_user_idle_idle_start[];
extern uint8_t _binary___obj_user_pingpong_pingpong_start[];

int
main(int argc, char **argv)
{
	pid_t init, idle, pingpong1, pingpong2, pid;
	struct msg msg;

	init = getpid();

	printf("init %d starts.\n");

	idle = spawn((uintptr_t) _binary___obj_user_idle_idle_start);
	printf("idle (pid %d) is created.\n", idle);

	pingpong1 = spawn((uintptr_t) _binary___obj_user_pingpong_pingpong_start);
	printf("pingpong (pid %d) is created.\n", pingpong1);

	pingpong2 = spawn((uintptr_t) _binary___obj_user_pingpong_pingpong_start);
	printf("pingpong (pid %d) is created.\n", pingpong2);

	printf("Send init pid %d to process %d.\n", init, pingpong1);
	send(pingpong1, &init, sizeof(pid_t));

	printf("Send init pid %d to process %d.\n", init, pingpong2);
	send(pingpong2, &init, sizeof(pid_t));

	if (recv(&msg)) {
		printf("Receive ACK from process %d.\n", msg.pid);
		pid = (msg.pid == pingpong2) ? pingpong1 : pingpong2;

		printf("Send pid %d to process %d.\n", pid, msg.pid);
		send(msg.pid, &pid, sizeof(pid_t));
	}

	if (recv(&msg)) {
		printf("Receive ACK from process %d.\n", msg.pid);
		pid = (msg.pid == pingpong2) ? pingpong1 : pingpong2;

		printf("Send pid %d to process %d.\n", pid, msg.pid);
		send(msg.pid, &pid, sizeof(pid_t));
	}

	return 0;
}
