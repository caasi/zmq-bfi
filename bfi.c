#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<zmq.h> /* 0MQ 2.2 */

#define CELL_LIMIT 3000

/* Convert C string to 0MQ string and send to socket */
char message[32];

static int
s_send (void *socket, char *string) {
    zmq_msg_t message;
    zmq_msg_init_size (&message, strlen (string));
    memcpy (zmq_msg_data (&message), string, strlen (string));
    int size = zmq_send (socket, &message, 0);
	zmq_msg_close(&message);
    return (size);
}

typedef struct Machine {
	void *zmq_context;
	void *zmq_publisher;
	unsigned int program_size;
	unsigned char *program;
	unsigned char *cell_ptr;
	unsigned char cells[CELL_LIMIT];
} Machine;

void machine_init(Machine* machine) {
	int i, rc;

	machine->zmq_context = zmq_init(2);
	machine->zmq_publisher = zmq_socket(machine->zmq_context, ZMQ_PUSH);
	rc = zmq_bind(machine->zmq_publisher, "ipc:///tmp/brainfuck.ipc");
	
	if (rc != 0) {
		fprintf(stderr, "can't bind to socket: %d\n", zmq_errno());
		exit(EXIT_FAILURE);
	}
	
	machine->cell_ptr = machine->cells;

	for (i = 0; i < CELL_LIMIT; ++i) {
		machine->cells[i] = 0;
	}

	return;
}

void machine_term(Machine* machine) {
	free(machine->program);
	zmq_close(machine->zmq_publisher);
	zmq_term(machine->zmq_context);
}

void machine_load(Machine* machine, FILE *source) {
	int c, i, size;

	fseek(source, 0, SEEK_END);
	size = ftell(source);
	fseek(source, 0, SEEK_SET);

	machine->program = (unsigned char *)malloc(sizeof(char) * size);

	i = 0;

	while((c = fgetc(source)) != EOF) {
		switch(c) {
			case '>':
			case '<':
			case '+':
			case '-':
			case '.':
			case ',':
			case '[':
			case ']':
				machine->program[i++] = (unsigned char)c;
				break;
			default:
				break;
		}
	}

	machine->program_size = i;

	return;
}

int machine_exec(Machine* machine, unsigned int start, unsigned int end) {
	unsigned int depth = 0, i, j;
	
	for (i = start; i < end; ++i) {
		sprintf(message, "EXEC:%ld", machine->cell_ptr - machine->cells);
		s_send(machine->zmq_publisher, message);

		usleep(33333); /* 30 fps */

		switch(machine->program[i]) {
			case '>':
				++machine->cell_ptr;
				if (machine->cell_ptr == &machine->cells[CELL_LIMIT]) {
					machine->cell_ptr = machine->cells;
				}
				break;
			case '<':
				--machine->cell_ptr;
				if (machine->cell_ptr < machine->cells) {
					machine->cell_ptr = &machine->cells[CELL_LIMIT];
				}
				break;
			case '+':
				++*machine->cell_ptr;

				sprintf(message, "SET:%c", *machine->cell_ptr);
				s_send(machine->zmq_publisher, message);
				break;
			case '-':
				--*machine->cell_ptr;

				sprintf(message, "SET:%c", *machine->cell_ptr);
				s_send(machine->zmq_publisher, message);
				break;
			case '.':
				putchar(*machine->cell_ptr);

				sprintf(message, "OUT:%c", *machine->cell_ptr);
				s_send(machine->zmq_publisher, message);
				break;
			case ',':
				*machine->cell_ptr = getchar();

				sprintf(message, "IN:%c", *machine->cell_ptr);
				s_send(machine->zmq_publisher, message);
				break;
			case '[':
				for (j = i + 1, depth = 0; machine->program[j] != ']' || depth != 0; ++j) {
					if (machine->program[j] == '[') ++depth;
					if (machine->program[j] == ']') --depth;
				}
				while (*machine->cell_ptr) {
					machine_exec(machine, i + 1, j);
				}
				i = j;
				break;
			default:
				break;
		}
	}
}

int main(int argc, char *argv[]) {
	FILE *source;
	Machine machine;

	if (argc != 2) {
		printf("usage: %s <filename>\n", argv[0]);
		return -1;
	}

	source = fopen(argv[1], "rb");

	if (source == NULL) {
		printf("can't find %s!\n", argv[1]);
		return -1;
	}

	machine_init(&machine);
	machine_load(&machine, source);

	sprintf(message, "START:%d", CELL_LIMIT);
	s_send(machine.zmq_publisher, message);

	machine_exec(&machine, 0, machine.program_size);

	sprintf(message, "END");
	s_send(machine.zmq_publisher, message);
	
	machine_term(&machine);

	fputc('\n', stdout);

	fclose(source);
}
