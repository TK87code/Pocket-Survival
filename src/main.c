#include "pocket.h"
#include "ext/pkt_win.h"
#include "defines.h"
#include "noise.h"
#include "data.h"
#include "map.h"
#include "render.h"
#include "player.h"
#include "pathfind.h"
#include "task.h"
#include <stdlib.h>	// atoi
#include <string.h>	// memset

enum scenes {
	SCENE_TITLE,
	SCENE_PLAY,
};

void game_init(void *user_data);
void game_update(void *user_data, float dt);
void game_draw(void *user_data);
static inline void handle_input(struct game_state *s, int key_code);
static void drag_start(struct game_state *s);
static void drag_update(struct game_state *s);
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

	biheap_init(&s->task_heap, s->task_heap_buffer, MAX_TASK, MIN_HEAP); 

	generate_map(s);	

	s->player = (struct player) {
		.wx = 40,
		.wy = 12,
		.state = PLAYER_STATE_IDLE,
		.current_task_id = -1,
		.wait_timer = 0,
		.carrying_item = ITEM_NONE,
		.carrying_item_amount = 0,
	};
}

void game_update(void *user_data, float dt)
{
	struct game_state *s = (struct game_state *)user_data;
	struct pkt_event e;

	while (pkt_poll_event(&e) == 0) {
		if (e.type == PKT_EVENT_KEY_PRESSED) 
			handle_input(s, e.data.key.key_code);
	}

	if (s->drag_ctx.bitflags & FLAG_DRAG_ACTIVE)
		drag_update(s);

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
		player_do_action(s);
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

	draw_terrains(s);
	draw_objects(s);
	draw_overlays(s);

	draw_items(s);
	draw_player(s);

	if (s->mode == MODE_DESIGNATE || s->mode == MODE_PILE)
		draw_cursor(s);
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
					s->drag_ctx.target_mask = 0x00000000;
					break;

				case 'p':
					s->mode = MODE_PILE;
					s->time_scale = 0.0f;
					s->drag_ctx.target_mask = 0x00000000;
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
					if ((s->drag_ctx.bitflags & FLAG_DRAG_ACTIVE) == 0 && s->drag_ctx.target_mask != 0)  
						drag_start(s);
					else if ((s->drag_ctx.bitflags & FLAG_DRAG_RESTRICTED) == 0)
						drag_end(s);
					break;

				case 't': s->drag_ctx.target_mask ^= (1 << OBJ_TREE); break;
				case 'r': s->drag_ctx.target_mask ^= (1 << OBJ_ROCK); break;
				case 'g': s->drag_ctx.target_mask ^= (1 << OBJ_GRASS); break;

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
					if ((s->drag_ctx.bitflags & FLAG_DRAG_ACTIVE) == 0 && s->drag_ctx.target_mask != 0) 
						drag_start(s);
					else 
						drag_end(s);
					break;

				case 'w': s->drag_ctx.target_mask ^= (1 << ITEM_WOOD); break;
				case 's': s->drag_ctx.target_mask ^= (1 << ITEM_STONE); break;		

				case 'h': s->cursor_lx -= 1; break;
				case 'l': s->cursor_lx += 1; break;
				case 'j': s->cursor_ly += 1; break;
				case 'k': s->cursor_ly -= 1; break;
			}
			break;
	}
}

static void drag_start(struct game_state *s)
{
	int cursor_wx = s->cursor_lx + s->cam_x;
	int cursor_wy = s->cursor_ly + s->cam_y;
	s->drag_ctx.bitflags |= FLAG_DRAG_ACTIVE;
	s->drag_ctx.start_wx = cursor_wx;
	s->drag_ctx.start_wy = cursor_wy;
}

static void drag_update(struct game_state *s)
{
	int cursor_wx = s->cursor_lx + s->cam_x;
	int cursor_wy = s->cursor_ly + s->cam_y;

	s->drag_ctx.min_wx = (cursor_wx <= s->drag_ctx.start_wx) ? cursor_wx : s->drag_ctx.start_wx;
	s->drag_ctx.min_wy = (cursor_wy <= s->drag_ctx.start_wy) ? cursor_wy : s->drag_ctx.start_wy;
	s->drag_ctx.max_wx = (s->drag_ctx.min_wx == s->drag_ctx.start_wx) ? cursor_wx : s->drag_ctx.start_wx;
	s->drag_ctx.max_wy = (s->drag_ctx.min_wy == s->drag_ctx.start_wy) ? cursor_wy : s->drag_ctx.start_wy;

	s->drag_ctx.bitflags &= ~FLAG_DRAG_RESTRICTED;

	if (s->mode == MODE_PILE) {
		for (int wy = s->drag_ctx.min_wy; wy <= s->drag_ctx.max_wy; wy++) {
			for (int wx = s->drag_ctx.min_wx; wx <= s->drag_ctx.max_wx; wx++) {
				uint8_t o = s->map.objects[get_map_index(wx, wy)];
				uint8_t flags = s->map.bitflags[get_map_index(wx,wy)];
				if (o != OBJ_NONE || flags != 0) {
					s->drag_ctx.bitflags |= FLAG_DRAG_RESTRICTED;
					return;
				}	
			}
		}
	}
}

static void drag_end(struct game_state *s)  
{
	const struct dragging_context *dc = &s->drag_ctx;
	for (int wy = dc->min_wy; wy <= dc->max_wy; wy++) {
		for (int wx = dc->min_wx; wx <= dc->max_wx; wx++) {
			unsigned int o = s->map.objects[get_map_index(wx, wy)];	
			switch (s->mode) {
				case MODE_DEFAULT:
					break;
					
				case MODE_DESIGNATE:
					if (o != OBJ_NONE && (dc->target_mask & (1 << o)))
						queue_task(s, wx, wy, object_defs[o].associated_task);
					break;

				case MODE_PILE:
					s->map.bitflags[get_map_index(wx, wy)] |= FLAG_CELL_PILE_AREA;
					break;
			}
		}
	}

	if (s->mode == MODE_PILE) {
		struct pile_area *pa = &s->pile_areas[s->pile_area_count];
		pa->min_wx = dc->min_wx;
		pa->min_wy = dc->min_wy;
		pa->max_wx = dc->max_wx;
		pa->max_wy = dc->max_wy;
		pa->accepted_items_mask = s->drag_ctx.target_mask;
		s->pile_area_count++;
	}

	memset(&s->drag_ctx, 0, sizeof(struct dragging_context)); 
}

static void return_to_mode_default(struct game_state *s)
{
	memset(&s->drag_ctx, 0, sizeof(struct dragging_context)); 
	s->mode = MODE_DEFAULT;
	s->time_scale = 1.0f;
	s->cursor_lx = VIEWPORT_COLS / 2;
	s->cursor_ly = VIEWPORT_ROWS / 2;
}
