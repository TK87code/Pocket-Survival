#include "player.h"
#include "defines.h"
#include "data.h"
#include "task.h"
#include "biheap.h"

static int player_random_walk(struct game_state *s, int eid);
static int player_do_idle_action(struct game_state *s, int eid);
static int player_step_path(struct game_state *s, int eid, int tx, int ty, int stop_dist, int *out_timer);
static int astar_cost_cb(int x, int y, void *user_data);
static int is_pilearea_available(struct game_state *s, unsigned int item_type, int *out_wx, int *out_wy);
static void remove_item(struct game_state *s, int index);
static inline int handle_state_move(struct game_state *s, int eid);
static inline void handle_harvest_task(struct game_state *s, int eid, int tid, int tx, int ty, unsigned int t_type);
static inline void handle_fetch_task(struct game_state *s, int eid, int tid, int tx, int ty);
static inline void handle_drop_task(struct game_state *s, int eid, int tid, int tx, int ty);

void player_do_action(struct game_state *s) 
{
	struct entity_data *e = &s->entities;
	int eid = 0; 
	int timer = e->wait_timer[eid];
	timer -= 1;

	if (timer <= 0) {
		switch (e->state[eid]) {
			case PLAYER_STATE_IDLE:  
				timer = player_do_idle_action(s, eid);
				break;

			case PLAYER_STATE_MOVE: 
				timer = handle_state_move(s, eid);
				break;

			case PLAYER_STATE_WORK: {
				int tid = e->current_task_id[eid];
				unsigned int t_type = (unsigned int)s->tasks.type[tid];

				uint16_t idx = s->tasks.target_map_idx[tid];
				int twx = IDX_TO_WX(idx);
				int twy = IDX_TO_WY(idx);

				switch (t_type) {
					case TASK_CHOP_TREE:
					case TASK_MINE_ROCK:
					case TASK_MOW_GRASS: 
						handle_harvest_task(s, eid, tid, twx, twy, t_type);
						break;

					case TASK_FETCH: 
						handle_fetch_task(s, eid, tid, twx, twy);
						break;

					case TASK_DROP: 
						handle_drop_task(s, eid, tid, twx, twy);
						break;
				}
				break;
			}
		}
	}
	e->wait_timer[eid] = timer;
}

#include <stdlib.h> // rand
		    
static int player_random_walk(struct game_state *s, int eid)
{
	struct entity_data *e = &s->entities;

	int dx = rand() % 3 - 1; // -1 ~ 1
	int dy = rand() % 3 - 1;
	int nx = e->wx[eid];
	int ny = e->wy[eid];

	if (e->wx[eid] + dx >= 0 && e->wx[eid] + dx < MAP_COLS)
		nx = e->wx[eid] + dx;
	if (e->wy[eid] + dy >= 0 && e->wy[eid] + dy < MAP_ROWS)
		ny = e->wy[eid] + dy;

	int idx = GET_IDX(nx, ny);
	uint8_t f = s->map.bitflags[idx];
	uint8_t t = s->map.terrains[idx];
	//TODO do I need this if? 
	if ((f & FLAG_CELL_OBSTRACT) == 0) {
		e->wx[eid] = nx; 
		e->wy[eid] = ny;

		return 2 * ((HUMAN_BASE_TICKS * terrain_defs[t].move_cost_percent) / 100);
	} else {
		return 10;
	}
}

static int astar_cost_cb(int x, int y, void *user_data)
{
	struct game_state *s = (struct game_state *)user_data;

	if (x < 0 || x >= MAP_COLS || y < 0 || y >= MAP_ROWS)
		return -1;

	int idx = GET_IDX(x, y);
	uint8_t f = s->map.bitflags[idx];
	uint8_t t = s->map.terrains[idx];

	if (f & FLAG_CELL_OBSTRACT) {
		return -1;
	}

	return terrain_defs[t].move_cost_percent / 100;
}

static int player_do_idle_action(struct game_state *s, int eid)
{
	struct entity_data *e = &s->entities;
	int timer = 10;
	int task_found = 0;
	unsigned int tid = 0;

	while (biheap_pop(&s->task_heap, &tid, NULL) == 0) {
		if (s->tasks.is_active[tid] == 0)
			continue;

		unsigned int ttype = (unsigned int)s->tasks.type[tid];
		uint16_t idx = s->tasks.target_map_idx[tid];

		if (ttype == TASK_FETCH) {
			unsigned int target_itype = ITEM_NONE;
			int target_iid = s->items.map_idx[idx];

			if (target_iid != INVALID_IDX)
				target_itype = s->items.type[target_iid];

			int is_target_none = (target_itype == ITEM_NONE);
			int is_free_pilearea = is_pilearea_available(s, target_itype, NULL, NULL);
			if (is_target_none || is_free_pilearea == 0) {
				s->tasks.is_active[tid] = 0;

				if (target_iid != INVALID_IDX)
					s->items.bitflags[target_iid] &= ~FLAG_ITEM_RESERVED;
				continue;
			}
		}

		e->current_task_id[eid] = tid;
		e->state[eid] = PLAYER_STATE_MOVE;
		task_found = 1;
		break;
	}

	if(!task_found)
		timer = player_random_walk(s, eid);

	return timer;
}

static int is_pilearea_available(struct game_state *s, unsigned int item_type, int *out_wx, int *out_wy)
{
	for (int i = 0; i < s->pile_area_count; i++) {
		struct pile_area *pa = &s->pile_areas[i];

		if (pa->accepted_items_mask & (1 << item_type)) {
			for (int wy = pa->min_wy; wy <= pa->max_wy; wy++) {
				for (int wx = pa->min_wx; wx <= pa->max_wx; wx++) {
					int idx = GET_IDX(wx, wy);
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
static int player_step_path(struct game_state *s, int eid, int tx, int ty, int stop_dist, int *out_timer)
{
	struct entity_data *e = &s->entities;	
	int16_t *plen = &e->path_len[eid];
	int16_t *pidx = &e->path_index[eid];

	//TODO if entity is on the cell already, move 1 cell?
	if (abs(e->wx[eid] - tx) + abs(e->wy[eid] - ty) <= stop_dist) {
		return 1;
	}

	if (e->path_len[eid] == 0) {
		struct astar_request req = {
			.out_path = e->path[eid],
			.user_data = s,
			.cost_cb = astar_cost_cb,
			.start = {e->wx[eid], e->wy[eid]},
			.end = {tx, ty},
			.max_path_len = 128,
		};
		*plen = astar_find_path(s->astar_ctx, &req);
		*pidx = 0;

		if (*plen == 0)
			return -1;
	}

	if (*pidx < *plen) {
		int nx = e->path[eid][*pidx].x;
		int ny = e->path[eid][*pidx].y;

		if (stop_dist > 0 && nx == tx && ny == ty) {
			*pidx = *plen;
		} else {
			unsigned int t = (unsigned int)s->map.terrains[GET_IDX(nx, ny)];
			e->wx[eid] = nx;
			e->wy[eid] = ny;
			*out_timer = (HUMAN_BASE_TICKS * terrain_defs[t].move_cost_percent) / 100;
			*pidx += 1;
		}
	}

	if (*pidx >= *plen) {
		if (abs(e->wx[eid] - tx) + abs(e->wy[eid] - ty) <= stop_dist) 
			return 1;
		else
			return -1;
	}

	return 0;
}

static void remove_item(struct game_state *s, int iid)
{
	int last = s->items.count - 1;
	uint16_t removed_map_idx = s->items.map_idx[iid];
	s->map.item_idx[removed_map_idx] = INVALID_IDX;

	if (iid != last) {
		s->items.map_idx[iid] = s->items.map_idx[last];
		s->items.amount[iid] = s->items.amount[last];
		s->items.type[iid] = s->items.type[last];
		s->items.bitflags[iid] = s->items.bitflags[last];

		uint16_t moved_map_idx = s->items.map_idx[iid];
		s->map.item_idx[moved_map_idx]= iid;
	}
	s->items.count -= 1;
}

static inline void spone_item(struct game_state *s, int tx, int ty, unsigned int t_type)
{
	int n_iid = s->items.count;
	uint16_t idx = GET_IDX(tx, ty);	

	s->items.map_idx[n_iid] = idx;
	s->items.type[n_iid] = drop_defs[t_type].item_type;
	s->items.amount[n_iid] = drop_defs[t_type].amount;
	s->items.bitflags[n_iid] = FLAG_ITEM_RESERVED;
	s->items.count += 1;

	s->map.item_idx[idx] = n_iid;
	s->map.bitflags[idx] |= FLAG_CELL_HAS_ITEM;

	queue_task(s, tx, ty, TASK_FETCH);
}

static inline int handle_state_move(struct game_state *s, int eid)
{
	struct entity_data *e = &s->entities;
	int tid = e->current_task_id[eid];
	unsigned int t_type = (unsigned int)s->tasks.type[tid];
	uint16_t idx = s->tasks.target_map_idx[tid];

	int twx = IDX_TO_WX(idx);
	int twy = IDX_TO_WY(idx);
	int stop_dist = task_defs[t_type].stop_dist;
	int timer = 0;

	int res = player_step_path(s, eid, twx,  twy, stop_dist, &timer);

	if (res == 1) {
		e->state[eid] = PLAYER_STATE_WORK;
		timer = task_defs[t_type].required_ticks;
		e->path_len[eid] = 0;
	} else if (res == -1) {
		s->tasks.is_active[tid] = 0;
		s->map.bitflags[idx] &= ~FLAG_CELL_MARKED;
		e->current_task_id[eid] = -1;
		e->state[eid] = PLAYER_STATE_IDLE;
		e->path_len[eid] = 0;
	}

	return timer;
}

static inline void handle_harvest_task(struct game_state *s, int eid, int tid, int tx, int ty, unsigned int t_type)
{
	int idx = GET_IDX(tx, ty);
	uint8_t *o = &s->map.objects[idx];
	uint8_t *f = &s->map.bitflags[idx];
	unsigned int t = s->map.terrains[idx];

	*o = (uint8_t)OBJ_NONE;
	*f &= (uint8_t)~FLAG_CELL_MARKED;
	if ((terrain_defs[t].bitflags & FLAG_CELL_OBSTRACT) == 0)
		*f &= ~FLAG_CELL_OBSTRACT;

	int is_task_productive = (task_defs[t_type].bitflags & FLAG_TASK_PRODUCTIVE);
	int can_item_spone = (s->items.count < MAX_DROPPED_ITEM);
	if (is_task_productive && can_item_spone) 
		spone_item(s, tx, ty, t_type);	

	struct entity_data *e = &s->entities;
	s->tasks.is_active[tid] = 0;
	e->current_task_id[eid] = -1;
	e->state[eid] = PLAYER_STATE_IDLE;
	e->path_len[eid] = 0;
}

static inline void handle_fetch_task(struct game_state *s, int eid, int tid, int tx, int ty)
{
	struct entity_data *e = &s->entities;
	uint16_t idx = GET_IDX(tx, ty);

	int iid = s->map.item_idx[idx];	

	if (iid != INVALID_IDX) {
		e->carrying_item[eid] = s->items.type[iid];
		e->carrying_item_amount[eid] = s->items.amount[iid];

		remove_item(s, iid);
		s->map.bitflags[idx] &= ~FLAG_CELL_HAS_ITEM;

		int pile_x, pile_y;
		if (is_pilearea_available(s, e->carrying_item[eid], &pile_x, &pile_y))
				queue_task(s, pile_x, pile_y, TASK_DROP); 

	} 

	s->tasks.is_active[tid] = 0;
	e->current_task_id[eid] = -1;
	e->state[eid] = PLAYER_STATE_IDLE;
}

static inline void handle_drop_task(struct game_state *s, int eid, int tid, int tx, int ty)
{
	struct entity_data *e = &s->entities;
	uint16_t idx = GET_IDX(tx, ty);

	if (s->items.count < MAX_DROPPED_ITEM) {
		int iid = s->items.count;
		s->items.map_idx[iid] = idx;
		s->items.type[iid] = e->carrying_item[eid];
		s->items.amount[iid] = e->carrying_item_amount[eid];
		s->items.bitflags[iid] = FLAG_ITEM_STORED;
		s->items.count += 1;

		s->map.item_idx[idx] = iid;
		s->map.bitflags[idx] |= FLAG_CELL_HAS_ITEM;
	}

	e->carrying_item[eid] = ITEM_NONE;
	e->carrying_item_amount[eid] = 0;
	e->current_task_id[eid] = -1;
	e->state[eid] = PLAYER_STATE_IDLE;
	s->tasks.is_active[tid] = 0;
}
