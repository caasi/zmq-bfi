#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<zmq.h> /* 0MQ 2.2 */

#define PROGRAM_LIMIT 1024
#define STACK_LIMIT 1024
#define CELL_LIMIT 3000

/* Convert C string to 0MQ string and send to socket */
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
	unsigned char *pc;
	unsigned char *tail;
	unsigned char program[PROGRAM_LIMIT];
	unsigned char **stack_ptr;
	unsigned char *stack[STACK_LIMIT];
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
	
	machine->pc = machine->program;
	machine->tail = machine->pc;
	machine->stack_ptr = machine->stack;
	machine->cell_ptr = machine->cells;

	for (i = 0; i < CELL_LIMIT; ++i) {
		machine->cells[i] = 0;
	}

	return;
}

void machine_term(Machine* machine) {
	zmq_close(machine->zmq_publisher);
	zmq_term(machine->zmq_context);
}

void machine_load(Machine* machine, FILE *source) {
	int c;
	unsigned int index = 0;

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
				*machine->pc++ = (unsigned char)c;
				break;
			default:
				break;
		}
	}

	machine->tail = machine->pc;
	machine->pc = machine->program;
	
	return;
}

int machine_exec(Machine* machine) {
	unsigned int depth = 0;
	unsigned char *next = machine->pc + 1;
	char message[32];

	if (machine->pc == machine->program) {
		sprintf(message, "BEGIN:%d", CELL_LIMIT);
		s_send(machine->zmq_publisher, message);
	}

	switch(*machine->pc) {
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

			sprintf(message, "INC");
			s_send(machine->zmq_publisher, message);
			break;
		case '-':
			--*machine->cell_ptr;

			sprintf(message, "DEC");
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
			if (*machine->cell_ptr == 0) {
				while(*next != ']' || depth != 0) {
					if (*next == '[') ++depth;
					if (*next == ']') --depth;
					++next;
				}
			} else {
				if (machine->stack_ptr >= &machine->stack[STACK_LIMIT]) {
					fprintf(stderr, "stack overflow!\n");
					exit(EXIT_FAILURE);
				}
				*machine->stack_ptr++ = next;
			}
			break;
		case ']':
			if (*machine->cell_ptr != 0) {
				next = *(machine->stack_ptr - 1);
			} else {
				--machine->stack_ptr;
			}
			break;
		default:
			break;
	}

	machine->pc = next;

	sprintf(message, "NEXT:%u", (unsigned int)(machine->pc - machine->program));
	s_send(machine->zmq_publisher, message);

	usleep(33333); /* 30 fps */

	if (machine->pc < machine->tail) {
		return 1;
	} else {
		sprintf(message, "END");
		s_send(machine->zmq_publisher, message);
		return 0;
	}
}

int main(int argc, char *argv[]) {
	FILE *source;
	Machine machine;

	if (argc != 2) {
		printf("usage: %s <filename>\n", argv[0]);
		return -1;
	}

	source = fopen(argv[1], "r");

	if (source == NULL) {
		printf("can't find %s!\n", argv[1]);
		return -1;
	}

	machine_init(&machine);
	machine_load(&machine, source);
	while(machine_exec(&machine));
	machine_term(&machine);

	fputc('\n', stdout);

	fclose(source);
}
