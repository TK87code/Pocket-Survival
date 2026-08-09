#ifndef PATHFIND_H
#define PATHFIND_H

#include <stddef.h>
#include <stdint.h>

/*
 * == A* Optimization Configuration ==
 * If this macro is enabled, the A* node structure will be compressed to 
 * 16 bytes. (max 65,535 map cell units).
 * If disabled, the data size will be 24 bytes(default) and it's safe for larger map size. 
 */
#ifdef ASTAR_OPTIMIZE_16BIT
	typedef uint16_t astar_index_t;
	typedef uint16_t astar_cost_t;
	typedef uint16_t astar_coord_t;
	#define ASTAR_INDEX_MAX UINT16_MAX
	#define ASTAR_COST_MAX UINT16_MAX
#else
	typedef uint32_t astar_index_t;
	typedef uint32_t astar_cost_t;
	typedef uint32_t astar_coord_t;
	#define ASTAR_INDEX_MAX UINT32_MAX
	#define ASTAR_COST_MAX UINT32_MAX
#endif

struct astar_context;

struct astar_pos { // 4 bytes in 16 bits mode, 8 bytes in 32 bits mode.
	astar_coord_t x;
	astar_coord_t y;
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

/**
 * @brief Initialize astar context. 
 *
 * @param  width Width of the conducting map
 * @param  height Height of the conducting map 
 * @param  buffer The pre-allocated memory buffer to store astar context.
 *
 * @return astar_context structure, or NULL if map size exceed this module limit.
 *         (Check A* Optimazation configulation if you enable it.)
 */
struct astar_context *astar_init(int width, int height, void *buffer);

struct astar_request { // 36 + 4 bytes in 16 bits mode, 44 + 4 bytes in 32 bits mode.
	struct astar_pos *out_path;	// A pre-allocated buffer to store the paths found.
	void *user_data;		// User data that will be passed to "cost_cb" callback
	astar_callback cost_cb;		// A callback to let function know the movement cost
	struct astar_pos start;		// X and Y coordinate of the start position.
	struct astar_pos end;		// X and Y coordinate of the end position.
	int32_t max_path_len; 		// Maximum length of path assumed.
};
	
/**
 * @brief  Conduct A* pathfinding. If the goal is unapproachable, 
 * the function stores the paths to the closest possible point.
 *
 * @param  ctx A pointer to astar_context structure.
 * @param  req A pointer to astar_request structure.
 * @return number of paths to the goal on success, -1 if the passed pointer is NULL.  
 */
int astar_find_path(struct astar_context *ctx, struct astar_request *req); 

#endif //PATHFIND_H
