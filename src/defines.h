#ifndef DEFINES_H
#define DEFINES_H

#include <stdint.h>
#include "ext/pkt_win.h"
#include "pathfind.h"
#include "biheap.h"

#define INVALID_IDX UINT16_MAX
#define ASTAR_OPTIMIZE_16BIT
#define BIHEAP_OPTIMIZE_16BIT

#define FPS 60

// Time and Time scales
#define REAL_SECONDS_PER_GAME_MINUTE 6
#define TICKS_PER_GAME_MINUTE (FPS * REAL_SECONDS_PER_GAME_MINUTE)

// Game world laws of physics
#define METERS_PER_CELL 2
#define HUMAN_SPEED_M_PER_MIN 60
#define CELLS_PER_MIN (HUMAN_SPEED_M_PER_MIN / METERS_PER_CELL)

#define HUMAN_BASE_TICKS (TICKS_PER_GAME_MINUTE / CELLS_PER_MIN)

#define TARGET_TICK_PER_SEC 60.0f
#define TICK_INTERVAL (1.0f / TARGET_TICK_PER_SEC)

#define SEC2TICK(seconds) ((uint16_t)((seconds) * TARGET_TICK_PER_SEC))

#define MAP_COLS 250 
#define MAP_ROWS 250 
#define PANEL_COLS 120
#define PANEL_ROWS 40
#define VIEWPORT_COLS 76
#define VIEWPORT_ROWS 38

// Helper macros
#define GET_IDX(wx, wy) (wy * MAP_COLS + wx)
#define IDX_TO_WX(idx) ((idx) % MAP_COLS)
#define IDX_TO_WY(idx) ((idx) / MAP_COLS)

enum command_mode {
	MODE_DEFAULT = 0,
	MODE_DESIGNATE,
	MODE_PILE,
};

enum terrain_type {
	TERRAIN_DEEP_WATER = 0,
	TERRAIN_GRAVEL,
	TERRAIN_SOIL,
	TERRAIN_MUD,
	TERRAIN_WATER,
	TERRAIN_SHALLOWS,
	TERRAIN_MOUNTAIN,
};

enum object_type {
	OBJ_ALL = 0,
	OBJ_NONE,
	OBJ_TREE,
	OBJ_ROCK,
	OBJ_GRASS,
};

enum item_type {
	ITEM_ALL = 0,
	ITEM_NONE,
	ITEM_WOOD,
	ITEM_STONE,
};

enum entity_type {
	ENT_PLAYER = 0,
	ENT_DOG,
};

enum player_state {
	PLAYER_STATE_IDLE = 0,
	PLAYER_STATE_MOVE,
	PLAYER_STATE_WORK,
};

enum task_type {
	TASK_CHOP_TREE,
	TASK_MINE_ROCK,
	TASK_MOW_GRASS,
	TASK_FETCH,
	TASK_DROP,
};

#define FLAG_CELL_OBSTRACT (1 << 0)
#define FLAG_CELL_HAS_ITEM (1 << 1)
#define FLAG_CELL_PILE_AREA (1 << 2)
#define FLAG_CELL_MARKED (1 << 3)

struct map_data { 
	uint8_t terrains[MAP_ROWS * MAP_COLS];	// enum terrain_type
	uint8_t objects[MAP_ROWS * MAP_COLS];		// enum object_type
	uint8_t bitflags[MAP_ROWS * MAP_COLS];	
	uint16_t item_idx[MAP_ROWS * MAP_COLS];
};

#define MAX_ASTAR_PATH 128

#define MAX_ENTITY 128

struct entity_data {  
	struct astar_pos path[MAX_ENTITY][MAX_ASTAR_PATH];
	int16_t count;
	int16_t path_len[MAX_ENTITY];
	int16_t path_index[MAX_ENTITY];
	int16_t wx[MAX_ENTITY];
	int16_t wy[MAX_ENTITY];
	int16_t wait_timer[MAX_ENTITY];
	int16_t current_task_id[MAX_ENTITY];	// current task id
	uint8_t state[MAX_ENTITY];			// current AI state
	uint8_t carrying_item[MAX_ENTITY];		// enum item_type
	uint8_t carrying_item_amount[MAX_ENTITY];
	uint8_t type[MAX_ENTITY];
};

#define MAX_DROPPED_ITEM 1024

#define FLAG_ITEM_STORED (1 << 0)
#define FLAG_ITEM_RESERVED (1 << 1)

struct item_data {
	int16_t count;
	uint16_t map_idx[MAX_DROPPED_ITEM];
	uint8_t amount[MAX_DROPPED_ITEM];
	uint8_t type[MAX_DROPPED_ITEM];
	uint8_t bitflags[MAX_DROPPED_ITEM];
};

#define MAX_TASK 255
struct task_data { 
	uint16_t target_map_idx[MAX_TASK];
	uint8_t type[MAX_TASK];		 //enum task_type
	uint8_t is_active[MAX_TASK];
	uint8_t count;	
};

#define MAX_PILE_AREA 128

struct pile_area { // 12 bytes
	uint32_t accepted_items_mask;
	int16_t min_wx;
	int16_t min_wy;
	int16_t max_wx;
	int16_t max_wy;
};

struct terrain_def { // 6 + 2 bytes
	uint16_t move_cost_percent;
	char sym;
	uint8_t fc;
	uint8_t bc;
	uint8_t bitflags;
};

struct entity_def { // 6 + 2 bytes
	uint16_t base_move_ticks; // how many ticks to rest till next move
	char sym;		
	uint8_t fc;
	uint8_t bc;
	uint8_t attr;
};

#define FLAG_OBJ_OBSTRACT (1 << 0)

struct object_def { // 15 + 1 bytes
	const char *sym_str;	// For multi-byte symbol 
	char sym_char;		// For 1 byte symbol
	uint8_t fc;
	uint8_t bc;
	uint8_t attr;
	uint8_t bitflags;
	uint8_t associated_task; // enum task_type
};

struct item_def { // 12 bytes
	const char *sym_str;
	char sym_char;
	uint8_t fc;
	uint8_t bc;
	uint8_t attr;
};

#define FLAG_TASK_PRODUCTIVE (1 << 0)
struct task_def { 
	int16_t base_prio_score;
	uint16_t required_ticks;
	int8_t stop_dist; // Specifies wether to top at the destination, or 1 cell away.
	uint8_t bitflags;
};

struct drop_def { // 2 bytes
	uint8_t item_type; // enum item_type
	uint8_t amount;
};

#define FLAG_DRAG_ACTIVE (1 << 0)
#define FLAG_DRAG_RESTRICTED (1 << 1)

struct dragging_context { // 13 + 3 bytes
	uint32_t target_mask;
	int16_t start_wx;
	int16_t start_wy;
	int16_t min_wx;
	int16_t min_wy;
	int16_t max_wx;
	int16_t max_wy;
	uint8_t bitflags;
};

struct game_state {
	struct map_data map;
	struct item_data items;
	struct task_data tasks;
	struct entity_data entities;
	struct pile_area pile_areas[MAX_PILE_AREA];
	struct pkt_window win_map;
	struct pkt_window win_status;
	struct pkt_window win_command;
	struct pkt_window win_log;
	struct dragging_context drag_ctx;

	struct astar_context *astar_ctx;

	struct biheap_node task_heap_buffer[MAX_TASK];
	struct biheap_manager task_heap;
	
	uint64_t global_ticks;

	float tick_accumulator;
	float time_scale;

	int task_count;
	int pile_area_count;

	int cursor_lx;
	int cursor_ly;

	int cam_x;
	int cam_y;
	
	int seed;
	enum command_mode mode;
};

#endif // DEFINES_H
