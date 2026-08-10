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
#define HUMAN_SPEED_M_PER_MIN 60
#define CELLS_PER_MIN (HUMAN_SPEED_M_PER_MIN / METERS_PER_CELL)

#define HUMAN_BASE_TICKS (TICKS_PER_GAME_MINUTE / CELLS_PER_MIN)

#define TARGET_TICK_PER_SEC 60.0f
#define TICK_INTERVAL (1.0f / TARGET_TICK_PER_SEC)

#define SEC2TICK(seconds) ((uint16_t)((seconds) * TARGET_TICK_PER_SEC))

#define MAP_COL 250 
#define MAP_ROW 250 
#define PANEL_COL 120
#define PANEL_ROW 40
#define VIEWPORT_COL 76
#define VIEWPORT_ROW 38

// Entity state flags

#define TEST_SEED 311

enum game_mode {
	MODE_DEFAULT = 0,
	MODE_DESIGNATE,
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
	OBJ_NONE = 0,
	OBJ_TREE,
	OBJ_ROCK,
	OBJ_GRASS,
};

#define MAX_DROPPED_ITEM 1024

enum item_type {
	ITEM_NONE = 0,
	ITEM_WOOD,
	ITEM_STONE,
};

enum entity_type {
	ENT_COLONIST = 0,
	ENT_DOG,
};

enum entity_state {
	ENT_STATE_IDLE = 0,
	ENT_STATE_MOVE,
	ENT_STATE_WORK,
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
};

#define FLAG_CELL_WALKABLE (1 << 0)
#define FLAG_CELL_HAS_ITEM (1 << 1)
#define FLAG_CELL_BLUE_PRINT (1 << 2)
#define FLAG_CELL_MARKED (1 << 3)

struct map_cell { // 3 + 1 bytes
	uint8_t object;		// Store enum object_type
	uint8_t terrain;	// store enum terrain_type
	uint8_t bitflags;	// Bitflags to store terrain state (e.g., roofed or burning)
};

#define FLAG_ENTITY_FRIENDLY (1 << 0)

struct entity { // 30 + 2 bytes 
	struct astar_pos path[128];
	int path_len;
	int path_index;
	int16_t x;
	int16_t y;
	int16_t wait_timer;
	uint16_t carrying_item_amount;
	int16_t current_task_id;	// current task id
	uint8_t type;			// store enum entity_type
	uint8_t bitflags;		// Bitflags to store entity state.
	uint8_t state;			// current AI state
	uint8_t carrying_item;		// enum item_type
};

struct item { // 7 + 1 bytes
	int16_t x;
	int16_t y;
	uint16_t amount;
	uint8_t type;
};

#define TASK_WAITING -1
#define TASK_ABORTED -2

struct task { // 6 + 2 bytes
	int16_t target_x;
	int16_t target_y;
	uint8_t type;		 //enum task_type
	int8_t assignee_id;
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

#define FLAG_OBJ_WALKABLE (1 << 0)
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

struct game_state {
	struct map_cell map[MAP_ROW][MAP_COL];
	struct item items[MAX_DROPPED_ITEM];
	struct task task_queue[MAX_TASK];
	struct entity entities[16];
	struct pkt_window win_map;
	struct pkt_window win_status;
	struct pkt_window win_command;
	struct pkt_window win_log;
	struct astar_context *astar_ctx;
	
	uint64_t global_ticks;

	float tick_accumulator;
	float time_scale;

	int entity_count;
	int task_count;
	int dropped_item_count;

	int cursor_lx;
	int cursor_ly;

	int cam_x;
	int cam_y;

	int is_dragging;
	int designate_start_wx;
	int designate_start_wy;

	int seed;
	enum game_mode mode;
};

#endif // DEFINES_H
