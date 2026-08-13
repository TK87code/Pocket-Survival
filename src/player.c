#include "player.h"
#include "defines.h"
#include "data.h"
#include "task.h"
#include <stdlib.h> // rand

static int player_random_walk(struct game_state *s);
static int astar_cost_cb(int x, int y, void *user_data);
static int player_do_idle_action(struct game_state *s);
static int search_dropped_item(struct game_state *s);
static int player_step_path(struct game_state *s, int tx, int ty, int stop_dist, int *out_timer);

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
					s->map[ts->target_wy][ts->target_wx].bitflags &= ~FLAG_CELL_MARKED;
					p->current_task_id = -1;
					p->state = PLAYER_STATE_IDLE;
					p->path_len = 0;
				}
				break;
			}		

			case PLAYER_STATE_WORK: {
				struct task *ts = &s->task_queue[p->current_task_id];
				struct map_cell *c =&s->map[ts->target_wy][ts->target_wx];

				if ((object_defs[c->object].bitflags & FLAG_CELL_OBSTRACT) && 
						((terrain_defs[c->terrain].bitflags & FLAG_CELL_OBSTRACT) == 0))
					c->bitflags &= ~FLAG_CELL_OBSTRACT;

				c->object = OBJ_NONE;
				c->bitflags &= ~FLAG_CELL_MARKED;

				if ((task_defs[ts->type].bitflags & FLAG_TASK_PRODUCTIVE) 
						&& (s->dropped_item_count < MAX_DROPPED_ITEM)) {
					struct item *itm = &s->items[s->dropped_item_count];
					itm->x = ts->target_wx;
					itm->y = ts->target_wy;
					itm->type = drop_defs[ts->type].item_type;
					itm->amount = drop_defs[ts->type].amount;
					s->dropped_item_count += 1;

					s->map[ts->target_wy][ts->target_wx].bitflags |= FLAG_CELL_HAS_ITEM;
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
							s->map[ts->target_wy][ts->target_wx].bitflags &= ~FLAG_CELL_HAS_ITEM;
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
					s->map[ts->dest_wy][ts->dest_wx].bitflags |= FLAG_CELL_HAS_ITEM;

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

	struct map_cell nc = s->map[ny][nx];

	if ((nc.bitflags & FLAG_CELL_OBSTRACT) == 0) {
		s->player.wx = nx; 
		s->player.wy = ny;

		return 2 * ((PLAYER_BASE_TICKS * terrain_defs[nc.terrain].move_cost_percent) / 100);
	} else {
		return 10;
	}
}

static int astar_cost_cb(int x, int y, void *user_data)
{
	struct game_state *s = (struct game_state *)user_data;

	if (x < 0 || x >= MAP_COLS || y < 0 || y >= MAP_ROWS)
		return -1;

	struct map_cell c = s->map[y][x];

	if (c.bitflags & FLAG_CELL_OBSTRACT) {
		return -1;
	}

	return terrain_defs[c.terrain].move_cost_percent / 100;
}

static int player_do_idle_action(struct game_state *s)
{
	int timer = 10;
	int task_found = 0;

	for (int j = 0; j < s->task_count; j++) {
		struct task *ts = &s->task_queue[j];
		if (ts->is_active == 1 && ts->type != TASK_HAUL) {
			s->player.current_task_id = j;
			s->player.state = PLAYER_STATE_MOVE;
			task_found = 1;
			break;
		}
	}

	if(!task_found && s->dropped_item_count > 0)
		task_found = search_dropped_item(s);	

	if(!task_found)
		timer = player_random_walk(s);

	return timer;
}

static int search_dropped_item(struct game_state *s)
{
	for (int i = 0; i < s->dropped_item_count; i++) {
		struct item *itm = &s->items[i];

		if (itm->amount == 0 || itm->is_stored == 1) 
			continue;	

		for (int j = 0; j < s->pile_area_count; j++) {
			struct pile_area *pa = &s->pile_areas[j];
			
			if ((pa->accepted_items_mask & (1 << itm->type)) == 0) 
				continue;

			int dest_x = -1, dest_y = -1;
			for (int py = pa->min_wy; py <= pa->max_wy; py++) {
				for (int px = pa->min_wx; px <= pa->max_wx; px++) {
					if ((s->map[py][px].bitflags & FLAG_CELL_HAS_ITEM) == 0) {
						dest_x = px;
						dest_y = py;
						break;
					}
				}
				if (dest_x != -1)
					break;
			}

			if (dest_x != -1) {
				int slot_index = -1;
				for (int k = 0; k < s->task_count; k++) {
					if (s->task_queue[k].is_active == 0) {
						slot_index = k;
						break;
					}

				}

				if (slot_index == -1 && s->task_count < MAX_TASK) {
					slot_index = s->task_count++;

				}

				if (slot_index != -1) {
					struct task *ts = &s->task_queue[slot_index];
					ts->type = TASK_HAUL;
					ts->target_wx = itm->x;
					ts->target_wy = itm->y;
					ts->dest_wx = dest_x;
					ts->dest_wy = dest_y;
					ts->is_active = 1;

					s->player.current_task_id = slot_index;
					s->player.state = PLAYER_STATE_HAUL_FETCH;
					return 1;
				}
			}
		}
	}

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
			struct map_cell nc = s->map[ny][nx];
			p->wx = nx;
			p->wy = ny;
			*out_timer = (PLAYER_BASE_TICKS * terrain_defs[nc.terrain].move_cost_percent) / 100;
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
