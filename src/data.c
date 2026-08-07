#include "data.h"
#include "defines.h"

struct terrain_def terrain_defs[] = {
	[TERRAIN_DEEP_WATER] = 	{0,   '~', 18, 16, 0x00},
	[TERRAIN_GRAVEL] = 	{100, ':', 244, 16, FLAG_WALKABLE},
	[TERRAIN_SOIL] = 	{100, '.', 137, 16, FLAG_WALKABLE},
	[TERRAIN_MUD] = 	{150, '=', 94, 16, FLAG_WALKABLE},
	[TERRAIN_WATER] = 	{0,   '~', 33, 16, 0x00},
	[TERRAIN_SHALLOWS] = 	{200, '-', 45, 16, FLAG_WALKABLE},
	[TERRAIN_MOUNTAIN] = 	{0,   '^', 250, 16, 0x00},
};

struct entity_def entity_defs[] = {
	[ENT_COLONIST] = 	{HUMAN_BASE_TICKS, '@', 15, 16, PKT_ATTR_NONE, },
	[ENT_DOG] = 		{(HUMAN_BASE_TICKS / 2),'d', 15, 16, PKT_ATTR_NONE},
};

struct object_def object_defs[] = {
	[OBJ_NONE] = 	{0},
	[OBJ_TREE] =	{"♣", 0, 22, 16, PKT_ATTR_BOLD, 0x00, 0x00}, 
	[OBJ_ROCK] =	{NULL, 'O', 236, 16, PKT_ATTR_BOLD, 0x00, FLAG_WALKABLE},
	[OBJ_GRASS] =	{NULL, '"', 76, 16, PKT_ATTR_NONE, 0x00, 0x00}, 
};

struct item_def item_defs[] = {
	[ITEM_NONE] = 	{0},
	[ITEM_WOOD] = 	{"≡", 0, 172, 16, PKT_ATTR_BOLD},
	[ITEM_STONE] =	{NULL, '*', 245, 16, PKT_ATTR_BOLD},
};

struct task_def task_defs[] = {
	[TASK_NONE] = 		{ WORK_NONE, 0},
	[TASK_CHOP_TREE] = 	{ WORK_FORESTRY, SEC2TICK(5.0)},
	[TASK_MINE_ROCK] = 	{ WORK_MINING, SEC2TICK(8.0)},
};

