#include "pocket.h"
#include "ext/pkt_win.h"
#include <stdint.h>	// uintx_ts
#include <stdlib.h>	// atoi

// === Definitions ===

#define MAP_COL 250 
#define MAP_ROW 250 
#define VIEWPORT_COL 72
#define VIEWPORT_ROW 36

#define FLAG_WALKABLE (1 << 0)

#define TEST_SEED 311

enum terrain_id {
	TERRAIN_DEEP_WATER = 0,
	TERRAIN_GRAVEL,
	TERRAIN_SOIL,
	TERRAIN_MUD,
	TERRAIN_WATER,
	TERRAIN_SHALLOWS,
	TERRAIN_MOUNTAIN,
};

enum object_id {
	OBJ_NONE = 0,
	OBJ_TREE,
	OBJ_ROCK,
	OBJ_GRASS,
};

struct terrain_def {
	enum pkt_color fcolor;
	enum pkt_color bcolor;
	char symbol;
};

struct map_cell { // 4 bytes
	uint8_t reserved;	// I added this to control save data size and contents
	uint8_t object;		// Store enum object_id
	uint8_t terrain;	// store enum terrain_id
	uint8_t bitflags;	// Bitflags to store terrain state (e.g., roofed or burning)
};

struct game_state {
	struct map_cell map[MAP_ROW][MAP_COL];
	int player_x;
	int player_y;
	int cursor_x;
	int cursor_y;

	int cam_x;
	int cam_y;

	struct pkt_window win_map;
	struct pkt_window win_status;
	struct pkt_window win_command;
	struct pkt_window win_log;

	int is_paused;
};

// === Globals ===

struct terrain_def terrains[] = {
	[TERRAIN_DEEP_WATER] = {(enum pkt_color)18, PKT_COLOR_BLACK, '~'},
	[TERRAIN_GRAVEL] = {(enum pkt_color)244, PKT_COLOR_BLACK, ':'},
	[TERRAIN_SOIL] = {(enum pkt_color)137, PKT_COLOR_BLACK, '.'},
	[TERRAIN_MUD] = {(enum pkt_color)94, PKT_COLOR_BLACK, '='},
	[TERRAIN_WATER] = {(enum pkt_color)33, PKT_COLOR_BLACK, '~'},
	[TERRAIN_SHALLOWS] = {(enum pkt_color)45, PKT_COLOR_BLACK, '-'},
	[TERRAIN_MOUNTAIN] = {(enum pkt_color)250, PKT_COLOR_BLACK, '^'},
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
static void draw_objects(char **str, char *sym, enum pkt_color *fc, unsigned int *attr, unsigned int o);
static void diversify_terrains(char *sym, enum pkt_color *fc, unsigned int t, uint32_t sudo_rando);	

int main(int argc, char *argv[]) 
{
	if (argc == 2)
		test_seed = atoi(argv[1]);

	struct game_state state = {0};

	struct pkt_config config = pkt_get_default_config();
	config.on_init = game_init;
	config.user_data = &state;

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

	state->is_paused = 0;

	generate_map(state);	

	state->player_x = 40;
	state->player_y = 12; 
	state->cursor_x = 41;
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
				state->player_y -= 1;
			if (e.data.key.key_code == 'j')
				state->player_y += 1;
			if (e.data.key.key_code == 'h')
				state->player_x -= 1;
			if (e.data.key.key_code == 'l')
				state->player_x += 1;
		}
	}

	if (state->player_x < 0)
		state->player_x = 0;
	if (state->player_x >= MAP_COL)
		state->player_x = MAP_COL - 1;
	if (state->player_y < 0)
		state->player_y = 0;
	if (state->player_y >= MAP_ROW)
		state->player_y = MAP_ROW - 1;

	state->cam_x = (state->player_x / VIEWPORT_COL) * VIEWPORT_COL;	
	state->cam_y = (state->player_y / VIEWPORT_ROW) * VIEWPORT_ROW;	

	if (state->cam_x < 0)
		state->cam_x = 0;
	if (state->cam_y < 0)
		state->cam_y = 0;
	if (state->cam_x > MAP_COL - VIEWPORT_COL)
		state->cam_x = MAP_COL - VIEWPORT_COL;
	if (state->cam_y > MAP_ROW - VIEWPORT_ROW)
		state->cam_y = MAP_ROW - VIEWPORT_ROW;
}

void game_draw(void *user_data)
{
	struct game_state *state = (struct game_state *)user_data;
	pkt_win_box(&state->win_command);

	pkt_win_puts(&state->win_command, 2, 0, " COMMANDS ");

	for (int y = 0; y < VIEWPORT_ROW; y++) {
		for (int x = 0; x < VIEWPORT_COL; x++) {
			int wx = state->cam_x + x;
			int wy = state->cam_y + y;
			unsigned int t = state->map[wy][wx].terrain;
			char sym = terrains[t].symbol;
			char *str = NULL;
			enum pkt_color fc = terrains[t].fcolor;

			uint32_t sudo_rando = ((uint32_t)wx * 374761393 ^ (uint32_t)wy * 668265263) % 100;
			
			diversify_terrains(&sym, &fc, t, sudo_rando);	

			unsigned int o = state->map[wy][wx].object;
			unsigned int attr = 0;

			draw_objects(&str, &sym, &fc, &attr, o);	
			
			if (!str)
				pkt_win_putc_color(&state->win_map, x, y, 
						fc, terrains[t].bcolor, attr, sym);
			else
				pkt_win_puts_color(&state->win_map, x, y, 
						fc, terrains[t].bcolor, attr, str); 
		}
	}

	pkt_win_putc_color(&state->win_map, state->player_x - state->cam_x, state->player_y - state->cam_y, 
			231, PKT_COLOR_BLACK, PKT_ATTR_NONE, '@');
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
			unsigned int obj = 0;
			
			float e_base = value_noise_2d(x, y, test_seed, 200.f) * 0.8f;
			float e_detail = value_noise_2d(x, y, test_seed + 123, 15.0f) * 0.2f;
			float elevation = e_base + e_detail;

			float m_base = value_noise_2d(x, y, test_seed + 1234, 150.0f) * 0.8f;
			float m_detail = value_noise_2d(x, y, test_seed + 12345, 10.0f) * 0.2f;
			float moisture = m_base + m_detail;

			float dent = value_noise_2d(x, y, test_seed + 999, 20.0f);

			float richness = value_noise_2d(x, y, test_seed + 444, 100.0f);
			float obj_dice = hash2d(x, y, test_seed);
			
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
					if (richness > 0.5f && obj_dice < 0.1f)
						obj = OBJ_TREE;
					else if (richness > 0.3f && obj_dice < 0.4f)
						obj = OBJ_GRASS;
				} else {
					t = TERRAIN_GRAVEL;
					if (richness > 0.5f && obj_dice < 0.1f)
						obj = OBJ_ROCK;
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
			s->map[y][x].object = (uint8_t)obj;
			s->map[y][x].bitflags = 0;
		}
	}
}

static void diversify_terrains(char *sym, enum pkt_color *fc, unsigned int t, uint32_t sudo_rando)
{
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

static void draw_objects(char **str, char *sym, enum pkt_color *fc, unsigned int *attr, unsigned int o)
{
	if (o == OBJ_TREE) {
		*str = "♣";
		*fc = 22;
		*attr = PKT_ATTR_BOLD;
	} else if (o == OBJ_ROCK) {
		*sym = 'O';
		*fc = 236;
		*attr = PKT_ATTR_BOLD;
	} else if (o == OBJ_GRASS) {
		*sym = '"';
		*fc = 76;
	}
}
