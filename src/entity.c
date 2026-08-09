#include "entity.h"
#include "defines.h"
#include "data.h"
#include <stdlib.h> // rand

static int entity_random_walk(struct game_state *s, int idx);
static int astar_cost_cb(int x, int y, void *user_data);

void entity_do_action(struct game_state *s) 
{
	for (int i = 0; i < s->entity_count; i++) {
		struct entity *e = &s->entities[i];
		int timer = e->wait_timer;
		timer -= 1;

		if (timer <= 0) {
			switch (e->state) {
				case ENT_STATE_IDLE: {
					if (e->type != ENT_COLONIST) {
						timer = entity_random_walk(s, i);
						break;
					}
					int task_found = 0;

					for (int j = 0; j < s->task_count; j++) {
						struct task *ts = &s->task_queue[j];
						if (ts->assignee_id == TASK_WAITING) {
							e->current_task_id = j;
							e->state = ENT_STATE_MOVE;
							ts->assignee_id = i;
							task_found = 1;
							break;
						}
					}

					if(!task_found)
						timer = entity_random_walk(s, i);

					break;
			 	} 
						     
				case ENT_STATE_MOVE: {
					struct task *ts = &s->task_queue[e->current_task_id];

					int dist_to_target = abs(e->x - ts->target_x) + abs(e->y - ts->target_y);
					if (dist_to_target <= 1) {
						e->state = ENT_STATE_WORK;
						timer = task_defs[ts->type].required_ticks;
						e->path_len = 0;
						break;
					}

					if (e->path_len == 0) {
						e->path_len = astar_find_path(
								s->astar_ctx,
								e->x, e->y, ts->target_x, ts->target_y,
								e->path, 128,
								astar_cost_cb, s);
						e->path_index = 0;

						if (e->path_len == 0) {
							ts->assignee_id = TASK_ABORTED;
							e->current_task_id = -1;
							e->state = ENT_STATE_IDLE;
							break;
						}
					}
					
					if (e->path_index < e->path_len) {
						int nx = e->path[e->path_index].x;
						int ny = e->path[e->path_index].y;

						if (nx == ts->target_x && ny == ts->target_y) {
							e->path_index = e->path_len;
						} else {
							struct map_cell nc = s->map[ny][nx];

							e->x = nx;
							e->y = ny;
							timer = (entity_defs[e->type].base_move_ticks * 
									terrain_defs[nc.terrain].move_cost_percent) / 100;
							e->path_index++;
						}
					}

					if (e->path_index >= e->path_len) {
						if (abs(e->x - ts->target_x) + abs(e->y - ts->target_y) <= 1) {
							e->state = ENT_STATE_WORK;
							timer = task_defs[ts->type].required_ticks;
						} else {
							ts->assignee_id = TASK_ABORTED;
							e->current_task_id = -1;
							e->state = ENT_STATE_IDLE;
						}

						e->path_len = 0;
					}
					break;
				}

				case ENT_STATE_WORK: {
					struct task *ts = &s->task_queue[e->current_task_id];
					s->map[ts->target_y][ts->target_x].object = OBJ_NONE;

					if ((task_defs[ts->type].bitflags & FLAG_TASK_PRODUCTIVE) 
							&& (s->dropped_item_count < MAX_DROPPED_ITEM)) {
						struct item *itm = &s->items[s->dropped_item_count];
						itm->x = ts->target_x;
						itm->y = ts->target_y;
						itm->type = drop_defs[ts->type].item_type;
						itm->amount = drop_defs[ts->type].amount;
						s->dropped_item_count += 1;
					}

					ts->assignee_id = TASK_ABORTED;
					e->current_task_id = -1;
					timer = 10;
					e->state = ENT_STATE_IDLE;
					e->path_len = 0;
					break;
				}
			}
		}

		e->wait_timer = timer;
	}
}

static int entity_random_walk(struct game_state *s, int idx)
{
	struct entity *e = &s->entities[idx];

	int dx = rand() % 3 - 1; // -1 ~ 1
	int dy = rand() % 3 - 1;
	int nx = e->x;
	int ny = e->y;

	if (e->x + dx >= 0 && e->x + dx < MAP_COL)
		nx = e->x + dx;
	if (e->y + dy >= 0 && e->y + dy < MAP_ROW)
		ny = e->y + dy;

	struct map_cell nc = s->map[ny][nx];

	if (nc.bitflags & FLAG_CELL_WALKABLE) {
		e->x = nx; 
		e->y = ny;

		return 2 * ((entity_defs[e->type].base_move_ticks *
				terrain_defs[nc.terrain].move_cost_percent) / 100);
	} else {
		return 10;
	}
}

static int astar_cost_cb(int x, int y, void *user_data)
{
	struct game_state *s = (struct game_state *)user_data;

	if (x < 0 || x >= MAP_COL || y < 0 || y >= MAP_ROW)
		return -1;

	struct map_cell c = s->map[y][x];

	if ((c.bitflags & FLAG_CELL_WALKABLE) == 0) {
		return -1;
	}

	return terrain_defs[c.terrain].move_cost_percent / 100;
}
