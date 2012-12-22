#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<zmq.h>

void *context;
void *receiver;

static char *
s_recv(void *socket) {
	int rc, size;
	zmq_msg_t message;
	rc = zmq_msg_init(&message);
	if (rc != 0) return NULL;
	rc = zmq_recv(socket, &message, 0);
	if (rc != 0) return NULL;
	size = zmq_msg_size(&message);
	if (size == 0) return NULL;
	char *string = malloc(size + 1);
	memcpy(string, zmq_msg_data(&message), size);
	zmq_msg_close(&message);
	string[size] = 0;
	return string;
}

int main(int argc, char *argv[]) {
	char *message;
	context = zmq_init(1);
	receiver = zmq_socket(context, ZMQ_PULL);
	zmq_connect(receiver, "ipc:///tmp/brainfuck.ipc");

	while(1) {
		message = s_recv(receiver);

		fflush(stdout);
		if (message) {
			printf("%s\n", message);
		}
	}

	zmq_close(receiver);
	zmq_term(context);

	return 0;
}
