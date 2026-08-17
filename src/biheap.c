#include "biheap.h"
// [REF] https://en.wikipedia.org/wiki/Binary_heap

static void up_heap(struct biheap_manager *manager, size_t curr);
static void down_heap(struct biheap_manager *manager, size_t curr);

int biheap_init(struct biheap_manager *manager, struct biheap_node *buffer, 
		size_t capacity, enum biheap_mode mode) 
{
	if (!manager)
		return -1;
	if (!buffer)
		return -2;

	manager->buffer = buffer;
	manager->n = 0;
	manager->capacity = capacity;
	manager->mode = mode;

	return 0;
}

int biheap_push(struct biheap_manager *manager, unsigned int id, int score) 
{
	if (!manager)
		return -1;

	if (manager->n >= manager->capacity)
		return -2;
	
	manager->buffer[manager->n] = (struct biheap_node) {(biheap_id_t)id, (biheap_score_t)score};
	size_t curr = manager->n;
	manager->n++;
	
	up_heap(manager, curr);

	return 0;
};

int biheap_pop(struct biheap_manager *manager, unsigned int *out_id, int *out_score)
{
	if (!manager)
		return -1;
	if (manager->n == 0)
		return -2;
	
	struct biheap_node popped = manager->buffer[0];
	if (out_id)
		*out_id = popped.id;
	if (out_score)
		*out_score = popped.score;
	
	manager->n--;
	manager->buffer[0] = manager->buffer[manager->n];

	down_heap(manager, 0);
	
	return 0;
}

int biheap_update(struct biheap_manager *manager, unsigned int id, int new_score)
{
	if (!manager)
		return -1;
	if (manager->n == 0)
		return -2;

	int old_score = 0;
	int node_found = 0;
	size_t target_id = 0; 

	for (size_t i = 0; i < manager->n; i++) {
		if (id == manager->buffer[i].id) {
			old_score = manager->buffer[i].score;
			manager->buffer[i].score = new_score;
			node_found = 1;
			target_id = i;
			break;
		}
	}

	if (node_found) {
		switch (manager->mode) {
			case MAX_HEAP: {
				if (new_score > old_score)
					up_heap(manager, target_id);
				else if (new_score < old_score)
					down_heap(manager, target_id);
				break;
			}

			case MIN_HEAP: {
				if (new_score > old_score)
					down_heap(manager, target_id);
				else if (new_score < old_score)
					up_heap(manager, target_id);
			}
		}
	} else {
		return -3;
	}
	
	return 0;
}

static void up_heap(struct biheap_manager *manager, size_t curr) 
{
	while (curr != 0) {
		size_t parent = (curr - 1) / 2;
		switch (manager->mode) {
			case MAX_HEAP:
				if (manager->buffer[parent].score >= manager->buffer[curr].score)
					return;
				break;
			case MIN_HEAP:
				if (manager->buffer[parent].score <= manager->buffer[curr].score)
					return;
				break;
		}
		struct biheap_node tmp = manager->buffer[parent];
		manager->buffer[parent] = manager->buffer[curr];
		manager->buffer[curr] = tmp;
		curr = parent;
	}
}

static void down_heap(struct biheap_manager *manager, size_t curr)
{
	while (1) {
		size_t child1 = (2 * curr) + 1;
		size_t child2 = (2 * curr) + 2;

		if (child1 >= manager->n) {
			break;
		}

		size_t target = child1;
		switch (manager->mode) {
			case MAX_HEAP: {
				if (child2 < manager->n && 
						manager->buffer[child2].score > manager->buffer[target].score)
					target= child2;

				if (manager->buffer[curr].score >= manager->buffer[target].score)
					return;
				break;
			}

			case MIN_HEAP: {
				if (child2 < manager->n &&
						manager->buffer[child2].score < manager->buffer[target].score)
					target = child2;

				if (manager->buffer[curr].score <= manager->buffer[target].score)
					return;
				break;
			}
		}

		struct biheap_node tmp = manager->buffer[curr];
		manager->buffer[curr] = manager->buffer[target];
		manager->buffer[target] = tmp;
		curr= target;
	}
}
