#include "pocket.h"
#include "ext/pkt_win.h"
#include <stdint.h>	// uintx_ts
#include <stdlib.h>	// atoi, rand

// === Definitions ===
#define FPS 60

#define MAP_COL 250 
#define MAP_ROW 250 
#define VIEWPORT_COL 72
#define VIEWPORT_ROW 36

// Entity state flags
#define FLAG_FRIENDLY (1 << 0)

#define TEST_SEED 311

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

enum task_type {
	TASK_NONE = 0,
	TASK_HARVEST,
	TASK_CONSTRUCT,
	TASK_HAUL,
};

#define FLAG_WALKABLE (1 << 0)
#define FLAG_HAS_ITEM (1 << 1)
#define FLAG_BLUE_PRINT (1 << 2)

struct map_cell { // 4 bytes
	uint8_t reserved;	
	uint8_t object;		// Store enum object_type
	uint8_t terrain;	// store enum terrain_type
	uint8_t bitflags;	// Bitflags to store terrain state (e.g., roofed or burning)
};

struct entity { // 16 bytes 
	int16_t x;
	int16_t y;
	int16_t wait_timer;
	uint16_t carrying_item_amount;
	int16_t reserved;
	int16_t current_task_id;	// current task id
	uint8_t type;			// store enum entity_type
	uint8_t bitflags;		// Bitflags to store entity state.
	uint8_t state;			// current AI state
	uint8_t carrying_item;		// enum item_type
};

struct item { // 8 bytes
	int16_t x;
	int16_t y;
	uint16_t amount;
	uint8_t item_type;
	int8_t reserved;
};

struct task { // 16 bytes
	int32_t reserved;
	int16_t reserved2;
	int16_t target_x;
	int16_t target_y;
	uint16_t req_amount;
	uint8_t req_item;
	uint8_t type;		 //enum task_type
	int8_t assignee_id;
	int8_t reserved3;
};

struct terrain_def { // 4 bytes
	char sym;
	uint8_t fc;
	uint8_t bc;
	uint8_t bitflags;
};

struct entity_def { // 4 bytes
	char sym;		
	uint8_t fc;
	uint8_t bc;
	uint8_t attr;
};

struct object_def { // 14 + 2 bytes
	const char *sym_str;	// For multi-byte symbol 
	char sym_char;		// For 1 byte symbol
	uint8_t fc;
	uint8_t bc;
	uint8_t attr;
	uint8_t add_bitflags;
	uint8_t clear_bitflags;
};

struct game_state {
	struct map_cell map[MAP_ROW][MAP_COL];
	struct item items[1024];
	struct task task_queue[256];
	struct entity entities[16];
	struct pkt_window win_map;
	struct pkt_window win_status;
	struct pkt_window win_command;
	struct pkt_window win_log;
	
	int entity_count;
	int task_count;
	int dropped_item_count;

	int cursor_x;
	int cursor_y;

	int cam_x;
	int cam_y;
};

// === Globals ===

struct terrain_def terrain_defs[] = {
	[TERRAIN_DEEP_WATER] = 	{'~', 18, 16, 0x00},
	[TERRAIN_GRAVEL] = 	{':', 244, 16, FLAG_WALKABLE},
	[TERRAIN_SOIL] = 	{'.', 137, 16, FLAG_WALKABLE},
	[TERRAIN_MUD] = 	{'=', 94, 16, FLAG_WALKABLE},
	[TERRAIN_WATER] = 	{'~', 33, 16, 0x00},
	[TERRAIN_SHALLOWS] = 	{'-', 45, 16, FLAG_WALKABLE},
	[TERRAIN_MOUNTAIN] = 	{'^', 250, 16, 0x00},
};

struct entity_def entity_defs[] = {
	[ENT_COLONIST] = 	{'@', 15, 16, PKT_ATTR_NONE},
	[ENT_DOG] = 		{'d', 15, 16, PKT_ATTR_NONE},
};

struct object_def object_defs[] = {
	[OBJ_NONE] = 	{0},
	[OBJ_TREE] =	{"♣", 0, 22, 16, PKT_ATTR_BOLD, 0x00, 0x00}, 
	[OBJ_ROCK] =	{NULL, 'O', 236, 16, PKT_ATTR_BOLD, 0x00, FLAG_WALKABLE},
	[OBJ_GRASS] =	{NULL, '"', 76, 16, PKT_ATTR_NONE, 0x00, 0x00}, 
};

int test_seed = 311;

// === Proto Types ===

// callbacks
void game_init(void *user_data);
void game_update(void *user_data, float dt);
void game_draw(void *user_data);
static float hash2d(int x, int y, int seed);
static float smooth_interp(float a, float b, float t);
static float value_noise_2d(float x, float y, int seed, float scale);
static void generate_map(struct game_state *s);
static void draw_terrains(struct game_state *s, int x, int y);
static void draw_objects(struct game_state *s, int x, int y);
static void diversify_terrains(int wx, int wy, char *sym, enum pkt_color *fc, unsigned int t);
static void draw_entities(struct game_state *s);
static void entity_do_action(struct game_state *s);
static void entity_random_walk(struct game_state *s, int idx);

int main(int argc, char *argv[]) 
{
	if (argc == 2)
		test_seed = atoi(argv[1]);

	struct game_state state = {0};

	struct pkt_config config = pkt_get_default_config();
	config.on_init = game_init;
	config.user_data = &state;
	config.target_fps = FPS;

	if (pkt_init(&config) < 0) {
		return -1;
	}
	
	struct pkt_scene scene = {0};
	scene.on_update = game_update;
	scene.on_draw = game_draw;
	scene.user_data = &state;

	pkt_register_scene(0, &scene);
	pkt_swap_scene(0);

	pkt_ignite();
	pkt_cleanup();

	return 0;
}

void game_init(void *user_data) 
{
	struct game_state *state = (struct game_state *)user_data;

	state->win_log = pkt_win_create(0, 0, 80, 1);
	state->win_map = pkt_win_create(4, 2, VIEWPORT_COL, VIEWPORT_ROW);
	state->win_status = pkt_win_create(0, 39, 80, 1);
	state->win_command = pkt_win_create(80, 0, 40, 40);

	generate_map(state);	

	state->entity_count = 2;

	state->entities[0] = (struct entity){
		.type = ENT_COLONIST,
		.x = 40,
		.y = 12,
		.state = ENT_STATE_IDLE,
		.current_task_id = -1,
		.wait_timer = FPS,
		.carrying_item = ITEM_NONE,
		.carrying_item_amount = 0,
		.bitflags = FLAG_FRIENDLY,
	};

	state->entities[1] = (struct entity){
		.type = ENT_DOG,
		.x = 41,
		.y = 12,
		.state = ENT_STATE_IDLE,
		.current_task_id = -1,
		.wait_timer = FPS,
		.carrying_item = ITEM_NONE,
		.carrying_item_amount = 0,
		.bitflags = FLAG_FRIENDLY,
	};

	state->cursor_x = 43;
	state->cursor_y = 12;
}

void game_update(void *user_data, float dt)
{
	struct game_state *state = (struct game_state *)user_data;
	(void)dt;
	struct pkt_event e;

	while (pkt_poll_event(&e) == 0) {
		if (e.type == PKT_EVENT_KEY_PRESSED) {
			if (e.data.key.key_code == PKT_KEY_ESCAPE) 
				pkt_quit();

			if (e.data.key.key_code == 'k')
				state->cursor_y -= 1;
			if (e.data.key.key_code == 'j')
				state->cursor_y += 1;
			if (e.data.key.key_code == 'h')
				state->cursor_x -= 1;
			if (e.data.key.key_code == 'l')
				state->cursor_x += 1;

			if (e.data.key.key_code == PKT_KEY_SPACE) {
				if (state->map[state->cursor_y][state->cursor_x].object == OBJ_TREE) {
					state->task_queue[state->task_count].type = TASK_HARVEST;
					state->task_queue[state->task_count].target_x = state->cursor_x;
					state->task_queue[state->task_count].target_y = state->cursor_y;
					state->task_queue[state->task_count].assignee_id = -1;
					state->task_count += 1;
				}
			}
		}
	}

	if (state->cursor_x < 0)
		state->cursor_x = 0;
	if (state->cursor_x >= MAP_COL)
		state->cursor_x = MAP_COL - 1;
	if (state->cursor_y < 0)
		state->cursor_y = 0;
	if (state->cursor_y >= MAP_ROW)
		state->cursor_y = MAP_ROW - 1;

	state->cam_x = (state->cursor_x / VIEWPORT_COL) * VIEWPORT_COL;	
	state->cam_y = (state->cursor_y / VIEWPORT_ROW) * VIEWPORT_ROW;	

	if (state->cam_x < 0)
		state->cam_x = 0;
	if (state->cam_y < 0)
		state->cam_y = 0;
	if (state->cam_x > MAP_COL - VIEWPORT_COL)
		state->cam_x = MAP_COL - VIEWPORT_COL;
	if (state->cam_y > MAP_ROW - VIEWPORT_ROW)
		state->cam_y = MAP_ROW - VIEWPORT_ROW;

	entity_do_action(state);	
}

void game_draw(void *user_data)
{
	struct game_state *state = (struct game_state *)user_data;
	pkt_win_box(&state->win_command);

	pkt_win_puts(&state->win_command, 2, 0, " COMMANDS ");

	for (int y = 0; y < VIEWPORT_ROW; y++) {
		for (int x = 0; x < VIEWPORT_COL; x++) {
			draw_terrains(state, x, y);
			draw_objects(state, x, y);
		}
	}
	
	draw_entities(state);
	
	pkt_win_putc_color(&state->win_map, state->cursor_x - state->cam_x, state->cursor_y - state->cam_y, 
			11, PKT_COLOR_BLACK, PKT_ATTR_BLINK, 'X');
}

// Return hashed sudo random float 0.0 - 1.0
static float hash2d(int x, int y, int seed) {
	uint32_t hash = (uint32_t)x * 374761393 + (uint32_t)y * 668265263 + (uint32_t)seed * 3266489917;
	hash = (hash ^ (hash >> 13)) * 1274126177;

	return (float)(hash & 0x00ffffff) / (float)0x00ffffff;
};

// Smooth interpolation using Quintic Hermite curve
// [REF] https://www.rose-hulman.edu/~finn/CCLI/Notes/day09.pdf
static float smooth_interp(float a, float b, float t) {
    float f = t * t * t * (t * (6.0f * t - 15.0f) + 10); 
    return a + f * (b - a);
}

// Generate smooth 2d noise using value noise algorithm
// scale: the size of grids
static float value_noise_2d(float x, float y, int seed, float scale)
{
	// Normalized
	float nx = x / scale;
	float ny = y / scale;
	
	// Grid Index
	int ix = (int)nx;
	int iy = (int)ny;
	
	// Ratio
	float tx = nx - (float)ix;
	float ty = ny - (float)iy;

	float h_top_left = hash2d(ix, iy, seed);
	float h_top_right = hash2d(ix + 1, iy, seed);
	float h_bot_left = hash2d(ix, iy + 1, seed);
	float h_bot_right = hash2d(ix + 1, iy + 1, seed);

	float top_blend = smooth_interp(h_top_left, h_top_right, tx);
	float bot_blend = smooth_interp(h_bot_left, h_bot_right, tx);

	return smooth_interp(top_blend, bot_blend, ty);
}

static void generate_map(struct game_state *s)
{
	for (int y = 0; y < MAP_ROW; y++) {
		for (int x = 0; x < MAP_COL; x++) {
			unsigned int o = 0;

			float e_base = value_noise_2d(x, y, test_seed, 200.f) * 0.8f;
			float e_detail = value_noise_2d(x, y, test_seed + 123, 15.0f) * 0.2f;
			float elevation = e_base + e_detail;

			float m_base = value_noise_2d(x, y, test_seed + 1234, 150.0f) * 0.8f;
			float m_detail = value_noise_2d(x, y, test_seed + 12345, 10.0f) * 0.2f;
			float moisture = m_base + m_detail;

			float dent = value_noise_2d(x, y, test_seed + 999, 20.0f);

			float richness = value_noise_2d(x, y, test_seed + 444, 100.0f);
			float o_dice = hash2d(x, y, test_seed);

			unsigned int t = TERRAIN_SOIL;

			if (elevation >= 0.7f) 
				t = TERRAIN_MOUNTAIN;
			else if (elevation >= 0.3f) {
				if (moisture >= 0.7f) { 
					if (dent > 0.8f)
						t = TERRAIN_SHALLOWS;
					else
						t = TERRAIN_MUD;
				} else if (moisture >= 0.3f) {
					t = TERRAIN_SOIL;
					if (richness > 0.5f && o_dice < 0.1f)
						o = OBJ_TREE;
					else if (richness > 0.3f && o_dice < 0.4f)
						o = OBJ_GRASS;
				} else {
					t = TERRAIN_GRAVEL;
					if (richness > 0.5f && o_dice < 0.1f)
						o = OBJ_ROCK;
				}
			} else {
				if (moisture >= 0.8f) 
					t = TERRAIN_DEEP_WATER;
				else if (moisture >= 0.6f)
					t = TERRAIN_WATER;
				else {
					if (dent > 0.8f)
						t = TERRAIN_SHALLOWS;
					else
						t = TERRAIN_MUD;
				}
			}

			s->map[y][x].terrain = (uint8_t)t;
			s->map[y][x].object = (uint8_t)o;
			s->map[y][x].bitflags = terrain_defs[t].bitflags;
			s->map[y][x].bitflags |= object_defs[o].add_bitflags;
			s->map[y][x].bitflags &= ~object_defs[o].clear_bitflags;
		}
	}
}


static void draw_terrains(struct game_state *s, int x, int y)
{
	int wx = s->cam_x + x;
	int wy = s->cam_y + y;
	unsigned int t = s->map[wy][wx].terrain;
	char sym = terrain_defs[t].sym;
	enum pkt_color fc = terrain_defs[t].fc;

	diversify_terrains(wx, wy, &sym, &fc, t);	

	pkt_win_putc_color(&s->win_map, x, y, fc, terrain_defs[t].bc, PKT_ATTR_NONE, sym);
}

static void diversify_terrains(int wx, int wy, char *sym, enum pkt_color *fc, unsigned int t)
{
	uint32_t sudo_rando = ((uint32_t)wx * 374761393 ^ (uint32_t)wy * 668265263) % 100;

	if (t == TERRAIN_SOIL) {
		if (sudo_rando < 10) {
			*sym = ',';
			*fc = 136;
		} else if (sudo_rando < 20) {
			*sym = '`';
			*fc = 138;
		}
	} else if (t == TERRAIN_GRAVEL) {
		if (sudo_rando < 15) {
			*sym = '.';
			*fc = 243;
		} else if (sudo_rando < 30) {
			*sym = ';';
			*fc = 245;
		}
	} else if (t == TERRAIN_DEEP_WATER || t == TERRAIN_WATER) {
		if (sudo_rando < 20) {
			*sym = '=';
			*fc = 27;
		}
	} else if (t == TERRAIN_MOUNTAIN) {
		if (sudo_rando < 15) {
			*fc = 235;
		} else if (sudo_rando < 25) {
			*fc = 239;
		}
	} else if (t == TERRAIN_MUD) {
		if (sudo_rando < 20) {
			*fc = 58;
		}
	}
}

static void draw_objects(struct game_state *s, int x, int y)
{
	int wx = s->cam_x + x;
	int wy = s->cam_y + y;
	unsigned int o = s->map[wy][wx].object;

	if (o != OBJ_NONE) {
		if (object_defs[o].sym_str != NULL)  
			pkt_win_puts_color(&s->win_map, x, y, 
					object_defs[o].fc, object_defs[o].bc, object_defs[o].attr, 
					object_defs[o].sym_str); 
		else
			pkt_win_putc_color(&s->win_map, x, y, 
					object_defs[o].fc, object_defs[o].bc, object_defs[o].attr, 
					object_defs[o].sym_char); 
	}

}

static void draw_entities(struct game_state *s)
{
	for (int i = 0; i < s->entity_count; i++) {
		struct entity e = s->entities[i];
		int lx = e.x - s->cam_x;
		int ly = e.y - s->cam_y;

		if (lx >= 0 && lx < VIEWPORT_COL && ly >= 0 && ly < VIEWPORT_ROW)
			pkt_win_putc_color(&s->win_map, lx, ly, 
					entity_defs[e.type].fc, entity_defs[e.type].bc, 
					PKT_ATTR_NONE, entity_defs[e.type].sym);
	}
}

static void entity_do_action(struct game_state *s) 
{
	for (int i = 0; i < s->entity_count; i++) {
		struct entity e = s->entities[i];
		int timer = e.wait_timer;
		timer -= 1;

		if (timer <= 0) {
			switch (e.state) {

				case ENT_STATE_IDLE:

					if (e.type == ENT_COLONIST) {
						int task_found = 0;

						for (int j = 0; j < s->task_count; j++) {
							if (s->task_queue[j].assignee_id == -1) {
								s->entities[i].current_task_id = j;
								s->entities[i].state = ENT_STATE_MOVE;
								s->task_queue[j].assignee_id = i;
								task_found = 1;
								break;
							}
						}

						if(!task_found)
							entity_random_walk(s, i);

					}else {
						entity_random_walk(s, i);
					}

					break;

				case ENT_STATE_MOVE:

					if (e.x == s->task_queue[e.current_task_id].target_x
							&& e.y == s->task_queue[e.current_task_id].target_y) {
						s->entities[i].state = ENT_STATE_WORK;
					} else {
						int nx = e.x;
						int ny = e.y;

						if (e.x < s->task_queue[e.current_task_id].target_x)
							nx = e.x + 1;
						else if (e.x > s->task_queue[e.current_task_id].target_x)
							nx = e.x - 1;

						if (e.y < s->task_queue[e.current_task_id].target_y)
							ny = e.y + 1;
						else if (e.y > s->task_queue[e.current_task_id].target_y)
							ny = e.y - 1;

						if (s->map[ny][nx].bitflags & FLAG_WALKABLE) {
							s->entities[i].x = nx;
							s->entities[i].y = ny;
						}
					}
					break;

				case ENT_STATE_WORK:
					int x = s->task_queue[e.current_task_id].target_x;
					int y = s->task_queue[e.current_task_id].target_y;
					s->map[y][x].object = OBJ_NONE;
					s->entities[i].current_task_id = -1;
					s->entities[i].state = ENT_STATE_IDLE;
					break;
			}
			timer = FPS;	
		}

		s->entities[i].wait_timer = timer;
	}
}

static void entity_random_walk(struct game_state *s, int idx)
{
	struct entity e = s->entities[idx];

	int dx = rand() % 3 - 1; // -1 ~ 1
	int dy = rand() % 3 - 1;
	int nx = e.x + dx;
	int ny = e.y + dy;

	if (s->map[ny][nx].bitflags & FLAG_WALKABLE) {
		if (nx >= 0 && nx < MAP_COL)
			s->entities[idx].x = nx; 
		if (ny >= 0 && ny < MAP_ROW)
			s->entities[idx].y = ny;
	}
}
