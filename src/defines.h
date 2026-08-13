#ifndef DEFINES_H
#define DEFINES_H

#include <stdint.h>
#include "ext/pkt_win.h"
#include "pathfind.h"

#define FPS 60

// Time and Time scales
#define REAL_SECONDS_PER_GAME_MINUTE 6
#define TICKS_PER_GAME_MINUTE (FPS * REAL_SECONDS_PER_GAME_MINUTE)

// Game world laws of physics
#define METERS_PER_CELL 2
#define PLAYER_SPEED_M_PER_MIN 60
#define CELLS_PER_MIN (PLAYER_SPEED_M_PER_MIN / METERS_PER_CELL)

#define PLAYER_BASE_TICKS (TICKS_PER_GAME_MINUTE / CELLS_PER_MIN)

#define TARGET_TICK_PER_SEC 60.0f
#define TICK_INTERVAL (1.0f / TARGET_TICK_PER_SEC)

#define SEC2TICK(seconds) ((uint16_t)((seconds) * TARGET_TICK_PER_SEC))

#define MAP_COLS 250 
#define MAP_ROWS 250 
#define PANEL_COLS 120
#define PANEL_ROWS 40
#define VIEWPORT_COLS 76
#define VIEWPORT_ROWS 38

#define MAX_ASTAR_PATH 128

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

#define MAX_DROPPED_ITEM 1024

enum item_type {
	ITEM_ALL = 0,
	ITEM_NONE,
	ITEM_WOOD,
	ITEM_STONE,
};

enum player_state {
	PLAYER_STATE_IDLE = 0,
	PLAYER_STATE_MOVE,
	PLAYER_STATE_WORK,
	PLAYER_STATE_HAUL_FETCH,
	PLAYER_STATE_HAUL_DELIVER,
};

#define MAX_TASK 256

enum work_type {
	WORK_FORESTRY,
	WORK_MINING,
	WORK_CONSTRUCTION,
};

enum task_type {
	TASK_CHOP_TREE,
	TASK_MINE_ROCK,
	TASK_MOW_GRASS,
	TASK_HAUL,
};

#define FLAG_CELL_OBSTRACT (1 << 0)
#define FLAG_CELL_HAS_ITEM (1 << 1)
#define FLAG_CELL_PILE_AREA (1 << 2)
#define FLAG_CELL_MARKED (1 << 3)

struct map_cell { // 3 + 1 bytes
	uint8_t object;		// enum object_type
	uint8_t terrain;	// enum terrain_type
	uint8_t bitflags;	
};

struct player { // bytes 
	struct astar_pos path[MAX_ASTAR_PATH];
	int16_t path_len;
	int16_t path_index;
	int16_t wx;
	int16_t wy;
	int16_t wait_timer;
	int16_t current_task_id;	// current task id
	uint8_t state;			// current AI state
	uint8_t carrying_item;		// enum item_type
	uint8_t carrying_item_amount;
};

struct item { // 8 bytes
	int16_t x;
	int16_t y;
	uint16_t amount;
	uint8_t type;
	uint8_t is_stored;
};

struct task { // 10 + 2 bytes
	int16_t target_wx;
	int16_t target_wy;
	int16_t dest_wx;
	int16_t dest_wy;
	uint8_t type;		 //enum task_type
	uint8_t is_active;
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
struct task_def { // 4 bytes
	uint16_t required_ticks;
	uint8_t work_type;
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
	struct map_cell map[MAP_ROWS][MAP_COLS];
	struct item items[MAX_DROPPED_ITEM];
	struct task task_queue[MAX_TASK];
	struct pile_area pile_areas[MAX_PILE_AREA];
	struct player player;
	struct pkt_window win_map;
	struct pkt_window win_status;
	struct pkt_window win_command;
	struct pkt_window win_log;
	struct astar_context *astar_ctx;
	struct dragging_context drag_ctx;
	
	uint64_t global_ticks;

	float tick_accumulator;
	float time_scale;

	int task_count;
	int dropped_item_count;
	int pile_area_count;

	int cursor_lx;
	int cursor_ly;

	int cam_x;
	int cam_y;
	
	int seed;
	enum command_mode mode;
};

#endif // DEFINES_H
