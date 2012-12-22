/* #include<allegro5/allegro5.h> */
#include<allegro5/allegro_primitives.h>
#include<zmq.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define CELL_WIDTH 10
#define CELL_HEIGHT 10
#define CELL_MAX_COL 60

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
	int redraw = 0;
	al_start_timer(timer);

	while(!done) {
		ALLEGRO_EVENT event;
		char *message = s_recv(receiver);

		if (message) {
			fflush(stdout);
			printf("%s\n", message);
		}

		al_wait_for_event(event_queue, &event);

		switch(event.type) {
			case ALLEGRO_EVENT_TIMER:
				redraw = 1;
				break;
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

		if (redraw && al_is_event_queue_empty(event_queue)) {
			redraw = 0;
			al_clear_to_color(al_map_rgb(0, 0, 0));
			al_draw_line(80, 60, 320, 240, al_map_rgb(255, 0, 0), 1);
			al_flip_display();
		}
	}
}

int main(int argc, char *argv[]) {
	init();
	game_loop();
	cleanup();

	return 0;
}
