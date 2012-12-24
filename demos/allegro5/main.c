/* #include<allegro5/allegro5.h> */
#include<allegro5/allegro_primitives.h>
#include<zmq.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define CELL_WIDTH 20
#define CELL_HEIGHT 20
#define CELL_MAX_COL 30

ALLEGRO_EVENT_QUEUE *event_queue;
ALLEGRO_TIMER *timer;
ALLEGRO_DISPLAY *display;
int done = 0;

void *context;
void *receiver;

static char *
s_recv(void *socket) {
	int rc, size;
	zmq_msg_t message;
	rc = zmq_msg_init(&message);
	if (rc != 0) return NULL;
	rc = zmq_recv(socket, &message, ZMQ_NOBLOCK);
	if (rc != 0) return NULL;
	size = zmq_msg_size(&message);
	if (size == 0) return NULL;
	char *string = malloc(size + 1);
	memcpy(string, zmq_msg_data(&message), size);
	zmq_msg_close(&message);
	string[size] = 0;
	return string;
}

void abort_game(const char *message) {
	printf("%s\n", message);
	exit(EXIT_FAILURE);
}

void init() {
	if (!al_init()) abort_game("Failed to init allegro.");
	if (!al_init_primitives_addon()) abort_game("Failed to init primitives.");
	if (!al_install_keyboard()) abort_game("Failed to install keyboard.");

	timer = al_create_timer(1.0 / 60);
	if (!timer) abort_game("Failed to create timer.");

	al_set_new_display_option(ALLEGRO_SINGLE_BUFFER, 1, ALLEGRO_REQUIRE);
	al_set_new_display_flags(ALLEGRO_WINDOWED);
	display = al_create_display(CELL_WIDTH * CELL_MAX_COL, CELL_HEIGHT);
	if (!display) abort_game("Failed to create display.");

	event_queue = al_create_event_queue();
	if (!event_queue) abort_game("Failed to create event queue.");

	al_register_event_source(event_queue, al_get_keyboard_event_source());
	al_register_event_source(event_queue, al_get_timer_event_source(timer));
	al_register_event_source(event_queue, al_get_display_event_source(display));

	done = 0;

	context = zmq_init(1);
	receiver = zmq_socket(context, ZMQ_PULL);
	zmq_connect(receiver, "ipc:///tmp/brainfuck.ipc");
}

void cleanup() {
	zmq_close(receiver);
	zmq_term(context);

	if (event_queue) al_destroy_event_queue(event_queue);
	if (display) al_destroy_display(display);
	if (timer) al_destroy_timer(timer);
	al_shutdown_primitives_addon();
}

void game_loop() {
	int redraw = 0, index = 0, data = 0;
	al_start_timer(timer);

	while(!done) {
		ALLEGRO_EVENT event;
		char *message = s_recv(receiver);
		char *part;
		int cell_num, height, x, y;

		if (message) {
			part = strtok(message, ":");
			if (part != NULL) {
				if (strncmp(part, "START", 5) == 0) {
					part = strtok(NULL, ":");
					cell_num = atoi(part);
					height = cell_num / CELL_MAX_COL;
					if (cell_num % CELL_MAX_COL != 0) {
						height += 1;
					}
					al_resize_display(display, CELL_MAX_COL * CELL_WIDTH, height * CELL_HEIGHT);
					al_clear_to_color(al_map_rgb(0, 0, 0));
				}
				if (strncmp(part, "EXEC", 4) == 0) {
					x = index % CELL_MAX_COL * CELL_WIDTH;
					y = index / CELL_MAX_COL * CELL_HEIGHT;
					al_draw_filled_rectangle(x, y, x + CELL_WIDTH, y + CELL_HEIGHT, al_map_rgb(data, data, data));
					part = strtok(NULL, ":");
					index = atoi(part);
					part = strtok(NULL, ":");
					data = atoi(part);
					x = index % CELL_MAX_COL * CELL_WIDTH;
					y = index / CELL_MAX_COL * CELL_HEIGHT;
					al_draw_filled_rectangle(x, y, x + CELL_WIDTH, y + CELL_HEIGHT / 10, al_map_rgb(255, 0, 0));
				}
				if (strncmp(part, "SET", 3) == 0) {
					part = strtok(NULL, ":");
					data = atoi(part);
					x = index % CELL_MAX_COL * CELL_WIDTH;
					y = index / CELL_MAX_COL * CELL_HEIGHT;
					al_draw_filled_rectangle(x, y, x + CELL_WIDTH, y + CELL_HEIGHT, al_map_rgb(data, data, data));
				}
				if (strncmp(part, "IN", 2) == 0) {
					part = strtok(NULL, ":");
					data = atoi(part);
					x = index % CELL_MAX_COL * CELL_WIDTH;
					y = index / CELL_MAX_COL * CELL_HEIGHT;
					al_draw_filled_rectangle(x, y, x + CELL_WIDTH, y + CELL_HEIGHT, al_map_rgb(data, data, 255));
				}
				if (strncmp(part, "OUT", 3) == 0) {
					part = strtok(NULL, ":");
					data = atoi(part);
					x = index % CELL_MAX_COL * CELL_WIDTH;
					y = index / CELL_MAX_COL * CELL_HEIGHT;
					al_draw_filled_rectangle(x, y, x + CELL_WIDTH, y + CELL_HEIGHT, al_map_rgb(data, 255, data));
				}
				if (strncmp(part, "END", 3) == 0) {
					done = 1;
				}
			}
		}

		al_flip_display();

		free(message);

		al_wait_for_event(event_queue, &event);

		switch(event.type) {
			case ALLEGRO_EVENT_KEY_DOWN:
				if (event.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
					done = 1;
				}
				break;
			case ALLEGRO_EVENT_DISPLAY_CLOSE:
				done = 1;
				break;
			default:
				break;
		}
	}
}

int main(int argc, char *argv[]) {
	init();
	game_loop();
	cleanup();

	return 0;
}
