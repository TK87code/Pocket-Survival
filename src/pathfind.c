#include "pathfind.h"
#include <stdint.h>
#include <limits.h> //UINT_MAX 
#include <string.h> //memcpy
#include <stdlib.h> //abs

#define FLAG_CLOSED (1 << 0)
#define FLAG_OPEN (1 << 1)

struct astar_node {
	unsigned int g; // real cost from start point to this node
	unsigned int f; // g + heuristic cost from this node to the goal.
	int x;		// node position in 2d space
	int y;
	int index;
	unsigned int search_id;
	struct astar_node* neighbors[4];
	struct astar_node* parent;
	uint8_t bitflags; 	
};

struct astar_context {
	int width;
	int height;
	unsigned int search_id;
	struct astar_node *nodes;
	struct astar_node **openlist;
};

size_t astar_get_req_memsize(int width, int height)
{
	size_t ctx_size = sizeof(struct astar_context);
	size_t nodes_size = sizeof(struct astar_node) * (width * height);
	size_t openlist_size = sizeof(struct astar_node *) * (width * height);

	return ctx_size + nodes_size + openlist_size; 
}

struct astar_context *astar_init(int width, int height, void *buffer)
{
	char *ptr = (char *)buffer;

	struct astar_context *ctx = (struct astar_context *)ptr;
	ptr += sizeof(struct astar_context);

	ctx->search_id = 0;

	ctx->nodes = (struct astar_node *)ptr;
	ptr += sizeof(struct astar_node) * (width * height);
	
	ctx->openlist = (struct astar_node **)ptr; 

	ctx->width = width;
	ctx->height = height;
	memset(ctx->nodes, 0, width * height * sizeof(struct astar_node));

	// connecting neibors
	for (int y = 0; y < ctx->height; y++) {
		for (int x = 0; x < ctx->width; x++) {
			struct astar_node *n = &ctx->nodes[y * ctx->width + x];

			n->x = x;
			n->y = y;

			if (y > 0)
				n->neighbors[0] = &ctx->nodes[(y - 1) * ctx->width + x];
			if (y < ctx->height - 1)
				n->neighbors[1] = &ctx->nodes[(y + 1) * ctx->width + x];
			if (x > 0)
				n->neighbors[2] = &ctx->nodes[y * ctx->width + (x - 1)];
			if (x < ctx->width - 1)
				n->neighbors[3] = &ctx->nodes[y * ctx->width + (x + 1)];
		}
	}

	return ctx;
}

static unsigned int distance(int sx, int sy, int ex, int ey)
{
	return abs(ex - sx) + abs(ey - sy);
}

static void heapify_up(struct astar_node **openlist, int index)
{
	int current = index;
	struct astar_node *n = openlist[current];

	while (current > 0) {
		int parent = (current - 1) / 2;
		struct astar_node *p = openlist[parent];

		if (n->f >= p->f)
			break;

		openlist[current] = p;
		openlist[parent] = n;
		p->index = current;
		n->index = parent;

		current = parent;
	}
}

static void push_openlist(struct astar_node **openlist, int *openlist_count, struct astar_node *n)
{
	int current = *openlist_count;
	openlist[current] = n;

	n->index = current;
	(*openlist_count)++;	

	heapify_up(openlist, current);	
}

static struct astar_node *pop_openlist(struct astar_node **openlist, int *openlist_count)
{
	if (*openlist_count == 0)
		return NULL;

	struct astar_node *rn = openlist[0];
	rn->index = -1;

	(*openlist_count)--;
	if (*openlist_count > 0) {
		openlist[0] = openlist[*openlist_count];
		openlist[0]->index = 0;
	}

	int current = 0;

	while (1) {
		int left = 2 * current + 1;
		int right = 2 * current + 2;
		int smallest = current;

		if (left < *openlist_count && openlist[left]->f < openlist[smallest]->f)
			smallest = left;
		if (right < *openlist_count && openlist[right]->f < openlist[smallest]->f)
			smallest = right;
		if (smallest == current)
			break;
		
		struct astar_node *tmp = openlist[current];
		openlist[current] = openlist[smallest];
		openlist[smallest] = tmp;

		openlist[current]->index = current;
		openlist[smallest]->index = smallest;

		current = smallest;
	}

	return rn;
}

static void init_node_if_needed(struct astar_context *ctx, struct astar_node *n)
{
	if (n->search_id != ctx->search_id) {
		n->g = UINT_MAX;
		n->f = UINT_MAX;
		n->parent = NULL;
		n->bitflags = 0;
		n->index = -1;
		n->search_id = ctx->search_id;
	}
}

int astar_find_path(struct astar_context *ctx, int start_x, int start_y, int end_x, int end_y, struct astar_pos *out_path, int max_path_len, astar_callback cost_cb, void *user_data) 
{
	ctx->search_id++;

	struct astar_node *sn = &ctx->nodes[start_y * ctx->width + start_x];
	struct astar_node *en = &ctx->nodes[end_y * ctx->width + end_x];

	init_node_if_needed(ctx, sn);

	sn->g = 0;
	sn->f = sn->g + distance(sn->x, sn->y, en->x, en->y);

	struct astar_node *closest_node = sn;
	unsigned int min_h = distance(sn->x, sn->y, en->x, en->y);

	int openlist_count = 0;
	push_openlist(ctx->openlist, &openlist_count, sn);

	while (openlist_count > 0) {
		struct astar_node *cn = pop_openlist(ctx->openlist, &openlist_count);	

		unsigned int current_h = distance(cn->x, cn->y, en->x, en->y);
		if (current_h < min_h) {
			min_h = current_h;
			closest_node = cn;
		}

		// checking if current node is the end node
		if (cn == en) {
			int idx = 0;
			while (cn->parent != NULL && idx < max_path_len) {
				out_path[idx].x = cn->x;
				out_path[idx].y = cn->y;
				cn = cn->parent;
				idx++;
			}	
			// reversing output array
			for (int i = 0; i < idx / 2; i++) {
				struct astar_pos tmp = out_path[i];
				out_path[i] = out_path[(idx - 1) - i];
				out_path[(idx - 1) - i] = tmp;
			}

			return idx;
		}

		cn->bitflags &= ~FLAG_OPEN;
		cn->bitflags |= FLAG_CLOSED;

		for (int j = 0; j < 4; j++) {
			struct astar_node *nn = cn->neighbors[j];

			if (nn == NULL)
				continue;

			init_node_if_needed(ctx, nn);	

			int move_cost = cost_cb(nn->x, nn->y, user_data);

			if (nn == en) {
				move_cost = 1;
			}

			if (move_cost < 0 || (nn->bitflags & FLAG_CLOSED))
				continue;

			unsigned int tentative_g = cn->g + move_cost; 
			if (tentative_g < nn->g) {
				nn->parent = cn;
				nn->g = tentative_g; 
				nn->f = nn->g + distance(nn->x, nn->y, en->x, en->y); 

				if ((nn->bitflags & FLAG_OPEN) == 0) {
					push_openlist(ctx->openlist, &openlist_count, nn);
					nn->bitflags |= FLAG_OPEN;
				} else {
					heapify_up(ctx->openlist, nn->index);
				}
			}
		}
	}

	if (closest_node != sn) {
		int idx = 0;
		struct astar_node *curr = closest_node;
		while (curr->parent != NULL && idx < max_path_len) {
			out_path[idx].x = curr->x;
			out_path[idx].y = curr->y;
			curr = curr->parent;
			idx++;
		}

		for (int i = 0; i < idx / 2; i++) {
			struct astar_pos tmp = out_path[i];
			out_path[i] = out_path[(idx - 1) - i];
			out_path[(idx - 1) - i] = tmp;
		}

		return idx;
	}

	return 0;
}
