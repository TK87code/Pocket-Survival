#include "pathfind.h"
#include <stdint.h>
#include <limits.h> //UINT_MAX 
#include <string.h> //memcpy
#include <stdlib.h> //abs

#define FLAG_CLOSED (1 << 0)
#define FLAG_OPEN (1 << 1)

struct astar_node { // 16 bytes in 16 bits mode, 24 bytes in 32 bits mode;
	uint32_t search_id;
	astar_cost_t g; // real cost from start point to this node
	astar_cost_t f; // g + heuristic cost from this node to the goal.
	astar_index_t heap_index;
	astar_index_t parent_map_index;
	uint8_t bitflags; 	
};

struct astar_context {
	struct astar_node **openlist;
	struct astar_node *nodes;
	int width;
	int height;
	int openlist_count;
	unsigned int search_id;
};

static inline void check_neighbors(struct astar_context *ctx, struct astar_node *en, struct astar_node *cn, 
		struct astar_request *req);
static inline int get_map_index(struct astar_context *ctx, struct astar_node *n);
static inline int get_x(struct astar_context *ctx, struct astar_node *n);
static inline int get_y(struct astar_context *ctx, struct astar_node *n);
static unsigned int distance(struct astar_context *ctx, struct astar_node *start, struct astar_node *end);
static void heapify_up(struct astar_node **openlist, int index);
static void push_openlist(struct astar_context *ctx, struct astar_node *n);
static struct astar_node *pop_openlist(struct astar_context *ctx);
static void init_node_if_needed(struct astar_context *ctx, struct astar_node *n);

size_t astar_get_req_memsize(int width, int height)
{
	size_t ctx_size = sizeof(struct astar_context);
	size_t nodes_size = sizeof(struct astar_node) * (width * height);
	size_t openlist_size = sizeof(struct astar_node *) * (width * height);

	return ctx_size + nodes_size + openlist_size; 
}

struct astar_context *astar_init(int width, int height, void *buffer)
{
	if ((size_t)(width * height) >= ASTAR_INDEX_MAX) 
		return NULL;

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

	return ctx;
}

int astar_find_path(struct astar_context *ctx, struct astar_request *req) 
{
	if (ctx == NULL || req == NULL)
		return -1;
	
	int num_steps = 0;

	ctx->search_id++;
	if (ctx->search_id == 0)
		ctx->search_id = 1;

	struct astar_node *sn = &ctx->nodes[req->start.y * ctx->width + req->start.x];
	struct astar_node *en = &ctx->nodes[req->end.y * ctx->width + req->end.x];

	init_node_if_needed(ctx, sn);

	sn->g = 0;
	unsigned int dist_stoe = distance(ctx, sn, en);
	sn->f = sn->g + dist_stoe; 

	struct astar_node *closest_node = sn;
	unsigned int min_h = dist_stoe; 

	ctx->openlist_count = 0;
	push_openlist(ctx, sn);

	while (ctx->openlist_count > 0) {
		struct astar_node *cn = pop_openlist(ctx);	

		unsigned int current_h = distance(ctx, cn, en);
		if (current_h < min_h) {
			min_h = current_h;
			closest_node = cn;
		}

		// checking if current node is the end node
		if (cn == en) {
			int idx = 0;
			while (cn->parent_map_index != ASTAR_INDEX_MAX && idx < req->max_path_len) {
				req->out_path[idx].x = get_x(ctx, cn);
				req->out_path[idx].y = get_y(ctx, cn);
				cn = &ctx->nodes[cn->parent_map_index];
				idx++;
			}	
			// reversing output array
			for (int i = 0; i < idx / 2; i++) {
				struct astar_pos tmp = req->out_path[i];
				req->out_path[i] = req->out_path[(idx - 1) - i];
				req->out_path[(idx - 1) - i] = tmp;
			}
			
			num_steps = idx;
			return num_steps;
		}

		cn->bitflags &= ~FLAG_OPEN;
		cn->bitflags |= FLAG_CLOSED;

		check_neighbors(ctx, en, cn, req);
	}

	if (closest_node != sn) {
		int idx = 0;
		struct astar_node *curr = closest_node;
		while (curr->parent_map_index != ASTAR_INDEX_MAX && idx < req->max_path_len) {
			req->out_path[idx].x = get_x(ctx, curr);
			req->out_path[idx].y = get_y(ctx, curr);
			curr = &ctx->nodes[curr->parent_map_index];
			idx++;
		}

		for (int i = 0; i < idx / 2; i++) {
			struct astar_pos tmp = req->out_path[i];
			req->out_path[i] = req->out_path[(idx - 1) - i];
			req->out_path[(idx - 1) - i] = tmp;
		}
		num_steps = idx;
		return num_steps;
	}

	return num_steps;
}

static inline void check_neighbors(struct astar_context *ctx, struct astar_node *en, struct astar_node *cn, struct astar_request *req)
{
	for (int j = 0; j < 4; j++) {
		struct astar_node *nn = NULL;
		int cn_x = get_x(ctx, cn);
		int cn_y = get_y(ctx, cn);

		if (j == 0 && cn_y > 0)
			nn = &ctx->nodes[(cn_y - 1) * ctx->width + cn_x];
		if (j == 1 && cn_y < ctx->height - 1)
			nn = &ctx->nodes[(cn_y + 1) * ctx->width + cn_x];
		if (j == 2 && cn_x > 0)
			nn = &ctx->nodes[cn_y * ctx->width + (cn_x - 1)];
		if (j == 3 && cn_x < ctx->width -1)
			nn = &ctx->nodes[cn_y * ctx->width + (cn_x + 1)];

		if (nn == NULL)
			continue;

		init_node_if_needed(ctx, nn);	

		int move_cost = req->cost_cb(get_x(ctx, nn), get_y(ctx, nn), req->user_data);

		if (nn == en) {
			move_cost = 1;
		}

		if (move_cost < 0 || (nn->bitflags & FLAG_CLOSED))
			continue;

		unsigned int tentative_g = cn->g + move_cost; 
		if (tentative_g < nn->g) {
			nn->parent_map_index = get_map_index(ctx,cn);
			nn->g = tentative_g; 
			nn->f = nn->g + distance(ctx, nn, en); 

			if ((nn->bitflags & FLAG_OPEN) == 0) {
				push_openlist(ctx, nn);
				nn->bitflags |= FLAG_OPEN;
			} else {
				heapify_up(ctx->openlist, nn->heap_index);
			}
		}
	}
}

static inline int get_map_index(struct astar_context *ctx, struct astar_node *n)
{
	return (int)(n - ctx->nodes);
}

static inline int get_x(struct astar_context *ctx, struct astar_node *n)
{
	return (int)get_map_index(ctx, n) % ctx->width;
}

static inline int get_y(struct astar_context *ctx, struct astar_node *n)
{
	return (int)get_map_index(ctx, n) / ctx->width;
}

static unsigned int distance(struct astar_context *ctx, struct astar_node *start, struct astar_node *end)
{
	return abs(get_x(ctx, end) - get_x(ctx, start)) + abs(get_y(ctx, end) - get_y(ctx, start));
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
		p->heap_index = current;
		n->heap_index = parent;

		current = parent;
	}
}

static void push_openlist(struct astar_context *ctx, struct astar_node *n)
{
	int current = ctx->openlist_count;
	ctx->openlist[current] = n;

	n->heap_index = current;
	(ctx->openlist_count)++;	

	heapify_up(ctx->openlist, current);	
}

static struct astar_node *pop_openlist(struct astar_context *ctx)
{
	if (ctx->openlist_count == 0)
		return NULL;

	struct astar_node *rn = ctx->openlist[0];
	rn->heap_index = ASTAR_INDEX_MAX;

	(ctx->openlist_count)--;
	if (ctx->openlist_count > 0) {
		ctx->openlist[0] = ctx->openlist[ctx->openlist_count];
		ctx->openlist[0]->heap_index = 0;
	}

	int current = 0;

	while (1) {
		int left = 2 * current + 1;
		int right = 2 * current + 2;
		int smallest = current;

		if (left < ctx->openlist_count && ctx->openlist[left]->f < ctx->openlist[smallest]->f)
			smallest = left;
		if (right < ctx->openlist_count && ctx->openlist[right]->f < ctx->openlist[smallest]->f)
			smallest = right;
		if (smallest == current)
			break;
		
		struct astar_node *tmp = ctx->openlist[current];
		ctx->openlist[current] = ctx->openlist[smallest];
		ctx->openlist[smallest] = tmp;

		ctx->openlist[current]->heap_index = current;
		ctx->openlist[smallest]->heap_index = smallest;

		current = smallest;
	}

	return rn;
}

static void init_node_if_needed(struct astar_context *ctx, struct astar_node *n)
{
	if (n->search_id != ctx->search_id) {
		n->g = ASTAR_COST_MAX;
		n->f = ASTAR_COST_MAX;
		n->parent_map_index = ASTAR_INDEX_MAX;
		n->heap_index = ASTAR_INDEX_MAX;
		n->bitflags = 0;
		n->search_id = ctx->search_id;
	}
}
