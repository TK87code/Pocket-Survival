#include "task.h"
#include "defines.h"
#include "data.h"
#include "biheap.h"

void queue_task(struct game_state *s, int wx, int wy, unsigned int task_type)
{
	unsigned int *f = (unsigned int *)&s->map.bitflags[get_map_index(wx, wy)];

	if (*f & FLAG_CELL_MARKED)
		return;

	int slot_index = -1;

	for (int i = 0; i < s->task_count; i++) {
		if (s->task_queue[i].is_active == 0) {
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
		ts->type = task_type;
		ts->target_wx = wx;
		ts->target_wy = wy;
		ts->dest_wx = -1;
		ts->dest_wy = -1;
		ts->is_active = 1;
		*f |= FLAG_CELL_MARKED;

		biheap_push(&s->task_heap, (unsigned int)slot_index, work_defs[ts->type].prio_score); 
	}
}
