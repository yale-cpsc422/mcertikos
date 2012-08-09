#include <proc.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
	pid_t pid = getpid();

	int known_init, known_partner;
	pid_t init_pid, partner_pid;
	struct msg msg;

	printf("pingpong %d.\n", pid);

	known_init = known_partner = 0;
	init_pid = partner_pid = 0;

	while (known_init == 0 || known_partner == 0) {
		recv(&msg);

		if (msg.pid == *(pid_t *) msg.data) {
			init_pid = msg.pid;
			known_init = 1;

			printf("pingping %d: send ACK to init %d.\n",
			       pid, init_pid);
			send(init_pid, &pid, sizeof(pid_t));
		}

		if (msg.pid != *(pid_t *) msg.data) {
			partner_pid = *(pid_t *) msg.data;
			known_partner = 1;
		}
	}

	printf("pingpong %d: init %d, partner %d.\n",
	       pid, init_pid, partner_pid);

	return 0;
}
