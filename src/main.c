#include "pocket.h"
#include "ext/pkt_win.h"
#include "defines.h"
#include "noise.h"
#include "data.h"
#include "map.h"
#include "render.h"
#include "entity.h"
#include "pathfind.h"
#include <stdlib.h>	// atoi

// === Proto Types ===

// callbacks
void game_init(void *user_data);
void game_update(void *user_data, float dt);
void game_draw(void *user_data);
static void queue_task(struct game_state *s);

int main(int argc, char *argv[]) 
{
	struct game_state state = {0};
	if (argc == 2)
		state.seed = atoi(argv[1]);

	struct pkt_config config = pkt_get_default_config();
	config.on_init = game_init;
	config.user_data = &state;
	config.target_fps = FPS;
	config.default_fcolor = 16;
	config.default_bcolor = 237;
	config.game_cols = PANEL_COL;
	config.game_rows = PANEL_ROW;

	if (pkt_init(&config) < 0) {
		return -1;
	}

	struct pkt_scene scene = {0};
	scene.on_update = game_update;
	scene.on_draw = game_draw;
	scene.user_data = &state;

	pkt_register_scene(0, &scene);
	pkt_swap_scene(0);

	pkt_ignite();
	pkt_cleanup();

	free(state.astar_ctx);
	return 0;
}

void game_init(void *user_data) 
{
	struct game_state *s = (struct game_state *)user_data;

	s->game_minutes_per_real_second = 1.0f; 
	s->tick_accumulator = 0.0f;
	s->time_scale = 1.0f;

	s->win_log = pkt_win_create(0, 0, 80, 1);
	s->win_map = pkt_win_create(2, 1, VIEWPORT_COL, VIEWPORT_ROW);
	s->win_status = pkt_win_create(0, 39, 80, 1);
	s->win_command = pkt_win_create(80, 0, 40, 40);

	size_t req_mem = astar_get_req_memsize(MAP_COL, MAP_ROW);
	void *astar_buffer = malloc(req_mem);
	s->astar_ctx = astar_init(MAP_COL, MAP_ROW, astar_buffer);

	generate_map(s);	

	s->entity_count = 2;

	s->entities[0] = (struct entity){
		.type = ENT_COLONIST,
		.x = 40,
		.y = 12,
		.state = ENT_STATE_IDLE,
		.current_task_id = -1,
		.wait_timer = FPS,
		.carrying_item = ITEM_NONE,
		.carrying_item_amount = 0,
		.bitflags = FLAG_ENTITY_FRIENDLY,
	};

	s->entities[1] = (struct entity){
		.type = ENT_DOG,
		.x = 41,
		.y = 12,
		.state = ENT_STATE_IDLE,
		.current_task_id = -1,
		.wait_timer = FPS,
		.carrying_item = ITEM_NONE,
		.carrying_item_amount = 0,
		.bitflags = FLAG_ENTITY_FRIENDLY,
	};

	s->cursor_x = 43;
	s->cursor_y = 12;
}

void game_update(void *user_data, float dt)
{
	struct game_state *s = (struct game_state *)user_data;
	struct pkt_event e;

	while (pkt_poll_event(&e) == 0) {
		if (e.type == PKT_EVENT_KEY_PRESSED) {
			if (e.data.key.key_code == PKT_KEY_ESCAPE) 
				pkt_quit();

			if (e.data.key.key_code == 'k')
				s->cursor_y -= 1;
			if (e.data.key.key_code == 'j')
				s->cursor_y += 1;
			if (e.data.key.key_code == 'h')
				s->cursor_x -= 1;
			if (e.data.key.key_code == 'l')
				s->cursor_x += 1;

			if (e.data.key.key_code == PKT_KEY_SPACE) {
				queue_task(s);	
			}
		}
	}

	if (s->cursor_x < 0)
		s->cursor_x = 0;
	if (s->cursor_x >= MAP_COL)
		s->cursor_x = MAP_COL - 1;
	if (s->cursor_y < 0)
		s->cursor_y = 0;
	if (s->cursor_y >= MAP_ROW)
		s->cursor_y = MAP_ROW - 1;

	s->cam_x = (s->cursor_x / VIEWPORT_COL) * VIEWPORT_COL;	
	s->cam_y = (s->cursor_y / VIEWPORT_ROW) * VIEWPORT_ROW;	

	if (s->cam_x < 0)
		s->cam_x = 0;
	if (s->cam_y < 0)
		s->cam_y = 0;
	if (s->cam_x > MAP_COL - VIEWPORT_COL)
		s->cam_x = MAP_COL - VIEWPORT_COL;
	if (s->cam_y > MAP_ROW - VIEWPORT_ROW)
		s->cam_y = MAP_ROW - VIEWPORT_ROW;

	s->tick_accumulator += dt * s->time_scale;

	while (s->tick_accumulator >= TICK_INTERVAL) {
		entity_do_action(s);
		s->global_ticks += 1;

		s->tick_accumulator -= TICK_INTERVAL;
	}
}

void game_draw(void *user_data)
{
	struct game_state *s = (struct game_state *)user_data;
	pkt_win_box_color(&s->win_command, 16, 237, PKT_ATTR_NONE);

	pkt_win_puts(&s->win_command, 2, 0, " COMMANDS ");
	pkt_win_puts(&s->win_log, 0, 0, " log ");
	draw_ingame_clock(s);

	for (int y = 0; y < VIEWPORT_ROW; y++) {
		for (int x = 0; x < VIEWPORT_COL; x++) {
			draw_terrains(s, x, y);
			draw_objects(s, x, y);
		}
	}

	draw_items(s);
	draw_entities(s);

	pkt_win_putc_color(&s->win_map, s->cursor_x - s->cam_x, s->cursor_y - s->cam_y, 
			11, 16, PKT_ATTR_BLINK, 'X');
}

static void queue_task(struct game_state *s)
{
	unsigned int o = s->map[s->cursor_y][s->cursor_x].object;
	if (o == OBJ_NONE)
		return;

	int slot_index = -1;
	for (int i = 0; i < s->task_count; i++) {
		if (s->task_queue[i].assignee_id == TASK_ABORTED) {
			slot_index = i;
			break;
		}
	}

	if (slot_index == -1 && s->task_count < MAX_TASK) {
		slot_index = s->task_count;
		s->task_count += 1;
	}

	if (slot_index != -1) {
		struct task *ts = &s->task_queue[slot_index];
		ts->type = object_defs[o].associated_task;
		ts->target_x = s->cursor_x;
		ts->target_y = s->cursor_y;
		ts->assignee_id = TASK_WAITING;
	}
}
