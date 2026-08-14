#include "biheap.h"
// [REF] https://en.wikipedia.org/wiki/Binary_heap

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

	while (curr != 0) {
		size_t parent = (curr - 1) / 2;
		switch (manager->mode) {
			case MAX_HEAP:
				if (manager->buffer[parent].score >= manager->buffer[curr].score)
					return 0;
				break;
			case MIN_HEAP:
				if (manager->buffer[parent].score <= manager->buffer[curr].score)
					return 0;
				break;
		}
		struct biheap_node tmp = manager->buffer[parent];
		manager->buffer[parent] = manager->buffer[curr];
		manager->buffer[curr] = tmp;
		curr = parent;
	}

	return 0;
};

