#include "entity.h"
#include "defines.h"
#include "data.h"
#include <stdlib.h> // rand

static int entity_random_walk(struct game_state *s, int idx);

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
						if (ts->assignee_id == -1) {
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
					//[REF] https://atmarkit.itmedia.co.jp/ait/articles/2405/16/news029.html
					int dx = abs(e->x - ts->target_x);
					int dy = abs(e->y - ts->target_y);

					if (dx <= 1 && dy <= 1) {
						e->state = ENT_STATE_WORK;
						timer = task_defs[ts->type].required_ticks;
					} else {
						int nx = e->x;
						int ny = e->y;

						if (e->x < ts->target_x)
							nx = e->x + 1;
						else if (e->x > ts->target_x)
							nx = e->x - 1;

						if (e->y < ts->target_y)
							ny = e->y + 1;
						else if (e->y > ts->target_y)
							ny = e->y - 1;

						struct map_cell nc = s->map[ny][nx];

						if (nc.bitflags & FLAG_CELL_WALKABLE) {
							e->x = nx;
							e->y = ny;
							timer = (entity_defs[e->type].base_move_ticks * 
									terrain_defs[nc.terrain].move_cost_percent) / 100; 
						}
					}
					break;
				}

				case ENT_STATE_WORK: {
					const struct task *ts = &s->task_queue[e->current_task_id];
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

					e->current_task_id = -1;
					timer = 10;
					e->state = ENT_STATE_IDLE;
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
	int nx = e->x + dx;
	int ny = e->y + dy;

	struct map_cell nc = s->map[ny][nx];

	if (nc.bitflags & FLAG_CELL_WALKABLE) {
		if (nx >= 0 && nx < MAP_COL)
			e->x = nx; 
		if (ny >= 0 && ny < MAP_ROW)
			e->y = ny;

		return 2 * ((entity_defs[e->type].base_move_ticks *
				terrain_defs[nc.terrain].move_cost_percent) / 100);
	} else {
		return 10;
	}
}
