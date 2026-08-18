#include "task.h"
#include "defines.h"
#include "data.h"
#include "biheap.h"

void queue_task(struct game_state *s, int wx, int wy, unsigned int task_type)
{
	uint8_t *f = &s->map.bitflags[GET_IDX(wx, wy)];

	if (*f & FLAG_CELL_MARKED)
		return;

	int tid = -1;

	for (int i = 0; i < MAX_TASK; i++) {
		if (s->tasks.is_active[i] == 0) {
			tid = i;
			break;
		}
	}

	if (tid != -1) {
		s->tasks.type[tid] = task_type;
		s->tasks.target_map_idx[tid] = (uint16_t)GET_IDX(wx, wy);
		s->tasks.is_active[tid] = 1;
		
		if (task_type == TASK_CHOP_TREE || task_type == TASK_MINE_ROCK || task_type == TASK_MOW_GRASS)
			*f |= FLAG_CELL_MARKED;

		biheap_push(&s->task_heap, (unsigned int)tid, task_defs[task_type].base_prio_score); 
	}
}
