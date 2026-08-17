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
static int is_pilearea_available(struct game_state *s, unsigned int item_type, int *out_wx, int *out_wy);
static void remove_item(struct game_state *s, int index);

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
				int res = player_step_path(s, p->move_dest_wx, p->move_dest_wy, p->stop_dist, &timer);
				int tid = p->current_task_id;

				if (res == 1) {
					p->state = PLAYER_STATE_WORK;
					timer = task_defs[s->tasks.type[tid]].required_ticks;
					p->path_len = 0;
				} else if (res == -1) {
					s->tasks.is_active[tid] = 0;
					s->map.bitflags[get_map_index(p->move_dest_wx, p->move_dest_wy)] &= ~FLAG_CELL_MARKED;
					p->current_task_id = -1;
					p->state = PLAYER_STATE_IDLE;
					p->path_len = 0;
				}
				break;
			}		

			case PLAYER_STATE_WORK: {
				int tid = p->current_task_id;
				unsigned int type = (unsigned int)s->tasks.type[tid];
				int tx = s->tasks.target_wx[tid];
				int ty = s->tasks.target_wy[tid];

				switch (type) {
					case TASK_CHOP_TREE:
					case TASK_MINE_ROCK:
					case TASK_MOW_GRASS: {
						int idx = get_map_index(tx, ty);
						uint8_t *o = &s->map.objects[idx];
						uint8_t *f = &s->map.bitflags[idx];
						unsigned int t = s->map.terrains[idx];
						
						if ((object_defs[*o].bitflags & FLAG_CELL_OBSTRACT) && 
								((terrain_defs[t].bitflags & FLAG_CELL_OBSTRACT) == 0))
							*f &= ~FLAG_CELL_OBSTRACT;

						*o = (uint8_t)OBJ_NONE;
						*f &= (uint8_t)~FLAG_CELL_MARKED;

						if ((task_defs[type].bitflags & FLAG_TASK_PRODUCTIVE) 
								&& (s->items.count < MAX_DROPPED_ITEM)) {
							int n_idx = s->items.count;
							s->items.wx[n_idx] = tx;
							s->items.wy[n_idx] = ty;
							s->items.type[n_idx] = drop_defs[type].item_type;
							s->items.amount[n_idx] = drop_defs[type].amount;
							s->items.bitflags[n_idx] = 0;
							s->items.count += 1;
							
							queue_task(s, tx, ty, TASK_FETCH);
							s->map.bitflags[idx] |= FLAG_CELL_HAS_ITEM;
						}

						s->tasks.is_active[tid] = 0;
						p->current_task_id = -1;
						timer = 10;
						p->state = PLAYER_STATE_IDLE;
						p->path_len = 0;
						break;
					}

					case TASK_FETCH: {
						int found_idx = -1;
						for (int i = 0; i < s->items.count; i++) {
							if (s->items.wx[i] == tx && s->items.wy[i] == ty) {
								p->carrying_item = s->items.type[i];
								p->carrying_item_amount = s->items.amount[i];
								found_idx = i;
								break;
							}
						}

						if (found_idx != -1) {
							remove_item(s, found_idx);
							s->map.bitflags[get_map_index(tx, ty)] &= ~FLAG_CELL_HAS_ITEM;
							int pile_x, pile_y;
							if (is_pilearea_available(s, p->carrying_item, &pile_x, &pile_y)) {
								queue_task(s, pile_x, pile_y, TASK_DROP); 
								s->tasks.is_active[tid] = 0;
								p->current_task_id = -1;
								p->state = PLAYER_STATE_IDLE;
							}
						} else {
							s->tasks.is_active[tid] = 0;
							p->current_task_id = -1;
							p->state = PLAYER_STATE_IDLE;
						}
						break;
					}

					case TASK_DROP: {
						if (s->items.count < MAX_DROPPED_ITEM) {
							int idx = s->items.count;
							s->items.wx[idx] = tx;
							s->items.wy[idx] = ty;
							s->items.type[idx] = p->carrying_item;
							s->items.amount[idx] = p->carrying_item_amount;
							s->items.bitflags[idx] = FLAG_ITEM_STORED;
							s->items.count += 1;

							s->map.bitflags[get_map_index(tx, ty)] |= FLAG_CELL_HAS_ITEM;
						}

						p->carrying_item = ITEM_NONE;
						p->carrying_item_amount = 0;

						s->tasks.is_active[tid] = 0;
						p->current_task_id = -1;
						p->state = PLAYER_STATE_IDLE;
						break;
					}
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
	struct player *p = &s->player;

	while (biheap_pop(&s->task_heap, &task_id, NULL) == 0) {
		if (s->tasks.is_active[task_id] == 0)
			continue;

		unsigned int type = (unsigned int)s->tasks.type[task_id];
		int tx = (int)s->tasks.target_wx[task_id];
		int ty = (int)s->tasks.target_wy[task_id];

		if (type == TASK_FETCH) {
			unsigned int target_item_type = ITEM_NONE;
			for (int i = 0; i < s->items.count; i++) {
				if (s->items.wx[i] == tx && s->items.wy[i] == ty) {
					target_item_type = s->items.type[i];
					break;
				}
			}

			if (target_item_type == ITEM_NONE || 
					!is_pilearea_available(s, target_item_type, NULL, NULL)) {
				s->tasks.is_active[task_id] = 0;
				continue;
			}
		}

		p->current_task_id = task_id;
		p->move_dest_wx = tx;
		p->move_dest_wy = ty;
		p->stop_dist = task_defs[type].stop_dist;

		p->state = PLAYER_STATE_MOVE;
		task_found = 1;
		break;
	}

	if(!task_found)
		timer = player_random_walk(s);

	return timer;
}

static int is_pilearea_available(struct game_state *s, unsigned int item_type, int *out_wx, int *out_wy)
{
	for (int p = 0; p < s->pile_area_count; p++) {
		struct pile_area *pa = &s->pile_areas[p];

		if (pa->accepted_items_mask & (1 << item_type)) {
			for (int wy = pa->min_wy; wy <= pa->max_wy; wy++) {
				for (int wx = pa->min_wx; wx <= pa->max_wx; wx++) {
					int idx = get_map_index(wx, wy);
					if ((s->map.bitflags[idx] & FLAG_CELL_HAS_ITEM) == 0 &&
							(s->map.bitflags[idx] & FLAG_CELL_OBSTRACT) == 0) {
						if (out_wx)
							*out_wx = wx;
						if (out_wy)
							*out_wy = wy;
						return 1;
					}
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

static void remove_item(struct game_state *s, int index)
{
	int last = s->items.count - 1;
	s->items.wx[index] = s->items.wx[last];
	s->items.wy[index] = s->items.wy[last];
	s->items.amount[index] = s->items.amount[last];
	s->items.type[index] = s->items.type[last];
	s->items.bitflags[index] = s->items.bitflags[last];
	s->items.count--;
}
