#include "data.h"
#include "defines.h"

struct terrain_def terrain_defs[] = {
	[TERRAIN_DEEP_WATER] = 	{0,   '~', 18, 16, FLAG_CELL_OBSTRACT},
	[TERRAIN_GRAVEL] = 	{100, ':', 244, 16, 0x00},
	[TERRAIN_SOIL] = 	{100, '.', 137, 16, 0x00},
	[TERRAIN_MUD] = 	{150, '=', 94, 16, 0x00},
	[TERRAIN_WATER] = 	{0,   '~', 33, 16, FLAG_CELL_OBSTRACT},
	[TERRAIN_SHALLOWS] = 	{200, '-', 45, 16, 0x00},
	[TERRAIN_MOUNTAIN] = 	{0,   '^', 250, 16, FLAG_CELL_OBSTRACT},
};

struct object_def object_defs[] = {
	[OBJ_ALL] =	{0},
	[OBJ_NONE] = 	{0},
	[OBJ_TREE] =	{NULL, 'Y', 94, 16, PKT_ATTR_BOLD, 0x00, TASK_CHOP_TREE}, 
	[OBJ_ROCK] =	{NULL, 'O', 58, 16, PKT_ATTR_BOLD, FLAG_OBJ_OBSTRACT, TASK_MINE_ROCK},
	[OBJ_GRASS] =	{NULL, '"', 76, 16, PKT_ATTR_NONE, 0x00, TASK_MOW_GRASS}, 
};

struct item_def item_defs[] = {
	[ITEM_NONE] = 	{0},
	[ITEM_WOOD] = 	{"≡", 0, 172, 16, PKT_ATTR_BOLD},
	[ITEM_STONE] =	{NULL, '*', 245, 16, PKT_ATTR_BOLD},
};

struct task_def task_defs[] = {
	[TASK_CHOP_TREE] = 	{SEC2TICK(5.0), WORK_FORESTRY, FLAG_TASK_PRODUCTIVE},
	[TASK_MINE_ROCK] = 	{SEC2TICK(8.0), WORK_MINING, FLAG_TASK_PRODUCTIVE},
	[TASK_MOW_GRASS] =	{SEC2TICK(1.0), WORK_FORESTRY, 0x00},
};

struct drop_def drop_defs[] = {
	[TASK_CHOP_TREE] = {ITEM_WOOD, 10},
	[TASK_MINE_ROCK] = {ITEM_STONE, 5},
};
