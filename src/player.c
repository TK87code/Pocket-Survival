#include "player.h"
#include "defines.h"
#include "data.h"
#include "task.h"
#include "biheap.h"
#include <stdlib.h> // rand

static int player_random_walk(struct game_state *s);
static int astar_cost_cb(int x, int y, void *user_data);
static int player_do_idle_action(struct game_state *s);
static int player_step_path(struct game_state *s, int tx, int ty, int stop_dist, int *out_timer);
static int is_pilearea_available(struct game_state *s);

void player_do_action(struct game_state *s) 
{
	struct player *p = &s->player;
	int timer = p->wait_timer;
	timer -= 1;

	if (timer <= 0) {
		switch (p->state) {
			case PLAYER_STATE_IDLE: { 
				timer = player_do_idle_action(s);
				break;
			}

			case PLAYER_STATE_MOVE: {
				struct task *ts = &s->task_queue[p->current_task_id];

				int res = player_step_path(s, ts->target_wx, ts->target_wy, 1, &timer);

				if (res == 1) {
					p->state = PLAYER_STATE_WORK;
					timer = task_defs[ts->type].required_ticks;
					p->path_len = 0;
				} else if (res == -1) {
					ts->is_active = 0;
					s->map.bitflags[get_map_index(ts->target_wx, ts->target_wy)] &= ~FLAG_CELL_MARKED;
					p->current_task_id = -1;
					p->state = PLAYER_STATE_IDLE;
					p->path_len = 0;
				}
				break;
			}		

			case PLAYER_STATE_WORK: {
				struct task *ts = &s->task_queue[p->current_task_id];
				int idx = get_map_index(ts->target_wx, ts->target_wy);
				uint8_t *o = &s->map.objects[idx];
				uint8_t *f = &s->map.objects[idx];
				uint8_t t = s->map.terrains[idx];
				

				if ((object_defs[*o].bitflags & FLAG_CELL_OBSTRACT) && 
						((terrain_defs[t].bitflags & FLAG_CELL_OBSTRACT) == 0))
					*f &= ~FLAG_CELL_OBSTRACT;

				*o = OBJ_NONE;
				*f &= ~FLAG_CELL_MARKED;

				if ((task_defs[ts->type].bitflags & FLAG_TASK_PRODUCTIVE) 
						&& (s->dropped_item_count < MAX_DROPPED_ITEM)) {
					struct item *itm = &s->items[s->dropped_item_count];
					itm->x = ts->target_wx;
					itm->y = ts->target_wy;
					itm->type = drop_defs[ts->type].item_type;
					itm->amount = drop_defs[ts->type].amount;
					s->dropped_item_count += 1;

					queue_task(s, itm->x, itm->y, TASK_HAUL);

					s->map.bitflags[get_map_index(itm->x, itm->y)] |= FLAG_CELL_HAS_ITEM;
				}

				ts->is_active = 0;
				p->current_task_id = -1;
				timer = 10;
				p->state = PLAYER_STATE_IDLE;
				p->path_len = 0;
				break;
			}

			case PLAYER_STATE_HAUL_FETCH: {
				struct task *ts = &s->task_queue[p->current_task_id];	

				int res = player_step_path(s, ts->target_wx, ts->target_wy, 0, &timer);

				if (res == 1) {
					for (int i = 0; i < s->dropped_item_count; i++) {
						struct item *itm = &s->items[i];
						if (itm->x == ts->target_wx && itm->y == ts->target_wy) {
							p->carrying_item = itm->type;
							p->carrying_item_amount = itm->amount;
							s->map.bitflags[get_map_index(ts->target_wx, ts->target_wy)] &= ~FLAG_CELL_HAS_ITEM;
							s->items[i] = s->items[s->dropped_item_count - 1];
							s->dropped_item_count -= 1;
							break;
						}
					}

					p->state = PLAYER_STATE_HAUL_DELIVER;
					p->path_len = 0;
				} else if (res == -1) {
					ts->is_active = 0;
					p->current_task_id = -1;
					p->state = PLAYER_STATE_IDLE;
					p->path_len = 0;
				}
				break;
			}

			case PLAYER_STATE_HAUL_DELIVER: {
				struct task *ts = &s->task_queue[p->current_task_id];
				
				int res = player_step_path(s, ts->dest_wx, ts->dest_wy, 0, &timer);

				if (res == 1) {
					s->map.bitflags[get_map_index(ts->dest_wx, ts->dest_wy)] |= FLAG_CELL_HAS_ITEM;

					if (s->dropped_item_count < MAX_DROPPED_ITEM) {
						struct item *itm = &s->items[s->dropped_item_count];
						itm->x = ts->dest_wx;
						itm->y = ts->dest_wy;
						itm->type = p->carrying_item;
						itm->amount = p->carrying_item_amount;
						itm->is_stored = 1;

						s->dropped_item_count += 1;
					}

					p->carrying_item = ITEM_NONE;
					p->carrying_item_amount = 0;

					ts->is_active = 0;
					p->current_task_id = -1;
					p->state = PLAYER_STATE_IDLE;
					p->path_len = 0;
				} else if (res == -1) {
					ts->is_active = 0;
					p->current_task_id = -1;
					p->state = PLAYER_STATE_IDLE;
					p->path_len = 0;
				}
				break;
			}
		}
	}

	p->wait_timer = timer;
}


static int player_random_walk(struct game_state *s)
{
	int dx = rand() % 3 - 1; // -1 ~ 1
	int dy = rand() % 3 - 1;
	int nx = s->player.wx;
	int ny = s->player.wy;

	if (s->player.wx + dx >= 0 && s->player.wx + dx < MAP_COLS)
		nx = s->player.wx + dx;
	if (s->player.wy + dy >= 0 && s->player.wy + dy < MAP_ROWS)
		ny = s->player.wy + dy;

	int idx = get_map_index(nx, ny);
	uint8_t f = s->map.bitflags[idx];
	uint8_t t = s->map.terrains[idx];

	if ((f & FLAG_CELL_OBSTRACT) == 0) {
		s->player.wx = nx; 
		s->player.wy = ny;

		return 2 * ((PLAYER_BASE_TICKS * terrain_defs[t].move_cost_percent) / 100);
	} else {
		return 10;
	}
}

static int astar_cost_cb(int x, int y, void *user_data)
{
	struct game_state *s = (struct game_state *)user_data;

	if (x < 0 || x >= MAP_COLS || y < 0 || y >= MAP_ROWS)
		return -1;

	int idx = get_map_index(x, y);
	uint8_t f = s->map.bitflags[idx];
	uint8_t t = s->map.terrains[idx];

	if (f & FLAG_CELL_OBSTRACT) {
		return -1;
	}

	return terrain_defs[t].move_cost_percent / 100;
}

static int player_do_idle_action(struct game_state *s)
{
	int timer = 10;
	int task_found = 0;
	unsigned int task_id = 0;

	while (biheap_pop(&s->task_heap, &task_id, NULL) == 0) {
		struct task *ts = &s->task_queue[task_id];

		if (ts->is_active == 1 && ts->type != TASK_HAUL) {
			s->player.current_task_id = (int16_t)task_id;
			s->player.state = PLAYER_STATE_MOVE;
			task_found = 1;
			break;
		} else if (ts->is_active == 1 && ts->type != TASK_HAUL) {
			if (is_pilearea_available(s))
				s->player.state = PLAYER_STATE_HAUL_FETCH;
		}
	}

	if(!task_found)
		timer = player_random_walk(s);

	return timer;
}

static int is_pilearea_available(struct game_state *s)
{
	(void)s;
	return 0;
}

// return 0 when entity is on the way, 1 when arrived, -1 no path found
static int player_step_path(struct game_state *s, int tx, int ty, int stop_dist, int *out_timer)
{
	struct player *p = &s->player;

	if (abs(p->wx - tx) + abs(p->wy - ty) <= stop_dist) {
		return 1;
	}

	if (p->path_len == 0) {
		struct astar_request req = {
			.out_path = p->path,
			.user_data = s,
			.cost_cb = astar_cost_cb,
			.start = {p->wx, p->wy},
			.end = {tx, ty},
			.max_path_len = 128,
		};
		p->path_len = astar_find_path(s->astar_ctx, &req);
		p->path_index = 0;

		if (p->path_len == 0)
			return -1;
	}

	if (p->path_index < p->path_len) {
		int nx = p->path[p->path_index].x;
		int ny = p->path[p->path_index].y;

		if (stop_dist > 0 && nx == tx && ny == ty) {
			p->path_index = p->path_len;
		} else {
			unsigned int t = (unsigned int)s->map.terrains[get_map_index(nx, ny)];
			p->wx = nx;
			p->wy = ny;
			*out_timer = (PLAYER_BASE_TICKS * terrain_defs[t].move_cost_percent) / 100;
			p->path_index++;
		}
	}

	if (p->path_index >= p->path_len) {
		if (abs(p->wx - tx) + abs(p->wy - ty) <= stop_dist) 
			return 1;
		else
			return -1;
	}

	return 0;
}
