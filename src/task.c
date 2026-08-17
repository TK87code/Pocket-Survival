#include "task.h"
#include "defines.h"
#include "data.h"
#include "biheap.h"

void queue_task(struct game_state *s, int wx, int wy, unsigned int task_type)
{
	uint8_t *f = &s->map.bitflags[get_map_index(wx, wy)];

	if (*f & FLAG_CELL_MARKED)
		return;

	int idx = -1;

	for (int i = 0; i < MAX_TASK; i++) {
		if (s->tasks.is_active[i] == 0) {
			idx = i;
			break;
		}
	}

	if (idx != -1) {
		s->tasks.type[idx] = task_type;
		s->tasks.target_wx[idx] = wx;
		s->tasks.target_wy[idx] = wy;
		s->tasks.is_active[idx] = 1;
		if (task_type == TASK_CHOP_TREE || task_type == TASK_MINE_ROCK || task_type == TASK_MOW_GRASS)
			*f |= FLAG_CELL_MARKED;

		biheap_push(&s->task_heap, (unsigned int)idx, task_defs[task_type].base_prio_score); 
	}
}
