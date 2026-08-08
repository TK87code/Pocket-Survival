#ifndef PATHFIND_H
#define PATHFIND_H

#include <stddef.h>

struct astar_context;

struct astar_pos { // 8 bytes 
	int x;
	int y;
};

/**
 * @brief A callback function to tell user defined movement cost of the cell to the algorithm.
 *
 * @param  x x coordinate
 * @param  y y coordinate
 * @param  user_data A pointer to the game data
 *
 * @return A movement cost of the specified cell. If it's an obstacle, return -1 to let the algorithm know. 
 */
typedef int (*astar_callback)(int x, int y, void *user_data);

/**
 * @brief  Get required memory size that user need to allocate.
 *
 * @param  width  Width of the field to conduct pathfinding.
 * @param  height  Height of the field to conduct pathfinging.
 *
 * @return Size of required memory.
 */
size_t astar_get_req_memsize(int width, int height);

struct astar_context *astar_init(int width, int height, void *buffer);
	
int astar_find_path(struct astar_context *ctx, int start_x, int start_y, int end_x, int end_y, struct astar_pos *out_path, int max_path_len, astar_callback cost_cb, void *user_data); 

#endif //PATHFIND_H
