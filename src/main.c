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

#define ASTAR_OPTIMIZE_16BIT

enum scenes {
	SCENE_TITLE,
	SCENE_PLAY,
};

void game_init(void *user_data);
void game_update(void *user_data, float dt);
void game_draw(void *user_data);
static void spone_entity(struct game_state *s, int type, int wx, int wy, unsigned int flag);
static void queue_task(struct game_state *s, int wx, int wy);
static inline void handle_input(struct game_state *s, int key_code);
static void drag_start(struct game_state *s);
static void drag_end(struct game_state *s);
static void return_to_mode_default(struct game_state *s);

int main(int argc, char *argv[]) 
{
	struct game_state *state = calloc(1, sizeof(struct game_state));
	if (!state)
		PKT_LOG(PKT_LOG_ERROR, "Failed to allocate memory.");

	state->seed = (argc == 2) ? atoi(argv[1]) : 97;

	struct pkt_config config = pkt_get_default_config();
	config.on_init = game_init;
	config.user_data = state;
	config.target_fps = FPS;
	config.default_fcolor = 16;
	config.default_bcolor = 237;
	config.game_cols = PANEL_COLS;
	config.game_rows = PANEL_ROWS;

	if (pkt_init(&config) < 0) {
		PKT_LOG(PKT_LOG_ERROR, "Failed to initialize Pocket game engine.");
		return -1;
	}

	struct pkt_scene scene = {0};
	scene.on_update = game_update;
	scene.on_draw = game_draw;
	scene.user_data = state;

	pkt_register_scene(SCENE_PLAY, &scene);
	pkt_swap_scene(SCENE_PLAY);

	pkt_ignite();
	pkt_cleanup();

	free(state->astar_ctx);
	free(state);

	return 0;
}

void game_init(void *user_data) 
{
	struct game_state *s = (struct game_state *)user_data;

	s->time_scale = 1.0f;
	s->win_log = pkt_win_create(0, 0, 80, 1);
	s->win_map = pkt_win_create(2, 1, VIEWPORT_COLS, VIEWPORT_ROWS);
	s->win_status = pkt_win_create(0, 39, 80, 1);
	s->win_command = pkt_win_create(80, 0, 40, 40);
	s->cursor_lx = VIEWPORT_COLS / 2;
	s->cursor_ly = VIEWPORT_ROWS / 2;

	size_t req_mem = astar_get_req_memsize(MAP_COLS, MAP_ROWS);
	void *astar_buffer = malloc(req_mem);
	s->astar_ctx = astar_init(MAP_COLS, MAP_ROWS, astar_buffer);

	generate_map(s);	

	spone_entity(s, ENT_COLONIST, 40, 12, FLAG_ENTITY_FRIENDLY);
	spone_entity(s, ENT_DOG, 41,12, FLAG_ENTITY_FRIENDLY);
}

void game_update(void *user_data, float dt)
{
	struct game_state *s = (struct game_state *)user_data;
	struct pkt_event e;

	while (pkt_poll_event(&e) == 0) {
		if (e.type == PKT_EVENT_KEY_PRESSED) 
			handle_input(s, e.data.key.key_code);
	}

	if (s->cursor_lx < 0)
		s->cursor_lx = 0;
	if (s->cursor_lx > VIEWPORT_COLS - 1)
		s->cursor_lx = VIEWPORT_COLS - 1;
	if (s->cursor_ly < 0)
		s->cursor_ly = 0;
	if (s->cursor_ly > VIEWPORT_ROWS - 1)
		s->cursor_ly = VIEWPORT_ROWS - 1;

	if (s->cam_x < 0)
		s->cam_x = 0;
	if (s->cam_y < 0)
		s->cam_y = 0;
	if (s->cam_x > MAP_COLS - VIEWPORT_COLS)
		s->cam_x = MAP_COLS - VIEWPORT_COLS;
	if (s->cam_y > MAP_ROWS - VIEWPORT_ROWS)
		s->cam_y = MAP_ROWS - VIEWPORT_ROWS;

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
	
	draw_command_box(s);
	draw_ingame_clock(s);
	draw_speed_indicator(s);

	for (int ly = 0; ly < VIEWPORT_ROWS; ly++) {
		for (int lx = 0; lx < VIEWPORT_COLS; lx++) {
			draw_terrains(s, lx, ly);
			draw_objects(s, lx, ly);
			draw_overlays(s, lx, ly);
		}
	}
	draw_items(s);
	draw_entities(s);

	if (s->mode == MODE_DESIGNATE || s->mode == MODE_PILE)
		draw_cursor(s);
}

static void queue_task(struct game_state *s, int wx, int wy)
{
	struct map_cell *c = &s->map[wy][wx];

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
		ts->type = object_defs[c->object].associated_task;
		ts->target_x = wx;
		ts->target_y = wy;
		ts->assignee_id = TASK_WAITING;
		c->bitflags |= FLAG_CELL_MARKED;
	}
}

static void handle_input(struct game_state *s, int key_code)
{
	if (key_code == PKT_KEY_UP)
		(s->time_scale == 3.0f) ? (s->time_scale = 3.0f) : (s->time_scale += 0.5f);
	if (key_code == PKT_KEY_DOWN)
		(s->time_scale == 0.0f) ? (s->time_scale = 0.0f) : (s->time_scale -= 0.5f);

	switch (s->mode) {
		case MODE_DEFAULT:
			switch (key_code) {
				case PKT_KEY_SPACE:
					(s->time_scale == 0.0f) ? (s->time_scale = 1.0f) : (s->time_scale = 0.0f);
					break;

				case 'd':
					s->mode = MODE_DESIGNATE;
					s->time_scale = 0.0f;
					s->drag_ctx.type = OBJ_ALL;
					break;

				case 'p':
					s->mode = MODE_PILE;
					s->time_scale = 0.0f;
					s->drag_ctx.type = ITEM_ALL;
					break;

				case 'h': s->cam_x -= 10; break;
				case 'l': s->cam_x += 10; break;
				case 'j': s->cam_y += 5; break;
				case 'k': s->cam_y -= 5; break;
			}
			break;

		case MODE_DESIGNATE:
			switch (key_code) {
				case PKT_KEY_ESCAPE:
					return_to_mode_default(s);
					break;
				case PKT_KEY_ENTER:
					if (s->drag_ctx.is_dragging == 0)  
						drag_start(s);
					else 
						drag_end(s);
					break;

				case 't': s->drag_ctx.type = OBJ_TREE; break;
				case 'r': s->drag_ctx.type = OBJ_ROCK; break;
				case 'g': s->drag_ctx.type = OBJ_GRASS; break;
				case 'a': s->drag_ctx.type = OBJ_ALL; break;

				case 'h': s->cursor_lx -= 1; break;
				case 'l': s->cursor_lx += 1; break;
				case 'j': s->cursor_ly += 1; break;
				case 'k': s->cursor_ly -= 1; break;
			}
			break;

		case MODE_PILE: 
			switch (key_code) {
				case PKT_KEY_ESCAPE:
					return_to_mode_default(s);
					break;

				case PKT_KEY_ENTER:
					if (s->drag_ctx.is_dragging == 0) 
						drag_start(s);
					else 
						drag_end(s);
					break;

				case 'w': s->drag_ctx.type = ITEM_WOOD; break;
				case 's': s->drag_ctx.type = ITEM_STONE; break;		

				case 'h': s->cursor_lx -= 1; break;
				case 'l': s->cursor_lx += 1; break;
				case 'j': s->cursor_ly += 1; break;
				case 'k': s->cursor_ly -= 1; break;
			}
			break;
	}
}

static void spone_entity(struct game_state *s, int type, int wx, int wy, unsigned int flag)
{
	if (s->entity_count >= MAX_ENTITY)
		return;

	s->entities[s->entity_count] = (struct entity){
		.type = type,
		.wx = wx,
		.wy = wy,
		.state = ENT_STATE_IDLE,
		.current_task_id = -1,
		.wait_timer = 0,
		.carrying_item = ITEM_NONE,
		.carrying_item_amount = 0,
		.bitflags = flag,
	};

	s->entity_count++;
}

static void drag_start(struct game_state *s)
{
	s->drag_ctx.is_dragging = 1;
	s->drag_ctx.start_wx = s->cursor_lx + s->cam_x;
	s->drag_ctx.start_wy = s->cursor_ly + s->cam_y;
}

static void drag_end(struct game_state *s)  
{
	int cursor_wx = s->cursor_lx + s->cam_x;
	int cursor_wy = s->cursor_ly + s->cam_y;
	int min_x = (cursor_wx <= s->drag_ctx.start_wx) ? cursor_wx : s->drag_ctx.start_wx;
	int min_y = (cursor_wy <= s->drag_ctx.start_wy) ? cursor_wy : s->drag_ctx.start_wy;
	int max_x = (min_x == s->drag_ctx.start_wx) ? cursor_wx : s->drag_ctx.start_wx;
	int max_y = (min_y == s->drag_ctx.start_wy) ? cursor_wy : s->drag_ctx.start_wy;

	for (int wy = min_y; wy <= max_y; wy++) {
		for (int wx = min_x; wx <= max_x; wx++) {
			struct map_cell *c = &s->map[wy][wx];
			if (s->mode == MODE_DESIGNATE){
				if (s->drag_ctx.type == OBJ_ALL && c->object != OBJ_NONE)
					queue_task(s, wx, wy);
				else if (c->object == s->drag_ctx.type)
					queue_task(s, wx, wy);
			} else if (s->mode == MODE_PILE) {
				c->bitflags |= FLAG_CELL_PILE_AREA;
				struct pile_area *pe = s->pile_areas[pile_area_count];
				pe->min_wx = min_wx;
				pe->min_wy = min_wy;
				pe->max_wx = max_wx;
				pe->max_wy = max_wy;
			}
		}
	}
	s->drag_ctx.is_dragging = 0;
}

static void return_to_mode_default(struct game_state *s)
{
	s->mode = MODE_DEFAULT;
	s->time_scale = 1.0f;
	s->cursor_lx = VIEWPORT_COLS / 2;
	s->cursor_ly = VIEWPORT_ROWS / 2;
}
