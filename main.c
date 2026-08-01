#include "pocket.h"
#include <stdint.h> // uintx_ts

// === Definitions ===

#define MAP_COL 80
#define MAP_ROW 24

#define FLAG_WALKABLE (1 << 0)

enum terrain_id {
	TERRAIN_DEEP_WATER = 0,
	TERRAIN_GRAVEL,
	TERRAIN_SOIL,
};

struct terrain_def {
	enum pkt_color fcolor;
	enum pkt_color bcolor;
	char symbol;
};

struct map_cell { // 4 bytes
	uint16_t reserved; // I added this to control save data size and contents
	uint8_t terrain; // store enum terrain_id
	uint8_t bitflags;
};


struct game_state {
	struct map_cell map[MAP_ROW][MAP_COL];
	int player_x;
	int player_y;
	int cursor_x;
	int cursor_y;
};

// === Globals ===

struct terrain_def terrains[] = {
	[TERRAIN_DEEP_WATER] = {PKT_COLOR_BLUE, PKT_COLOR_BLACK, '~'},
	[TERRAIN_GRAVEL] = {PKT_COLOR_YELLOW, PKT_COLOR_BLACK, ':'},
	[TERRAIN_SOIL] = {PKT_COLOR_GREEN, PKT_COLOR_BLACK, ','},
};

// === Proto Types ===

// callbacks
void game_init(void *user_data);
void game_update(void *user_data, float dt);
void game_draw(void *user_data);

int main(void) 
{
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

	for (int y = 0; y < MAP_ROW; y++) {
		for (int x = 0; x < MAP_COL; x++) {
			state->map[y][x].terrain = TERRAIN_DEEP_WATER;
			state->map[y][x].bitflags = 0;
		}
	}

	state->player_x = 40;
	state->player_y = 12; 
	state->cursor_x = 41;
	state->cursor_y = 12;
}

void game_update(void *user_data, float dt)
{
	struct game_state *state = (struct game_state *)user_data;
	(void)state;
	(void)dt;
	struct pkt_event e;

	while (pkt_poll_event(&e) == 0) {
		if (e.type == PKT_EVENT_KEY_PRESSED) {
			if (e.data.key.key_code == PKT_KEY_ESCAPE) 
				pkt_quit();
		}
	}
}

void game_draw(void *user_data)
{
	struct game_state *state = (struct game_state *)user_data;
	
	for (int y = 0; y < MAP_ROW; y++) {
		for (int x = 0; x < MAP_COL; x++) {
			uint8_t id = state->map[y][x].terrain;
			struct terrain_def def = terrains[id];
			pkt_putc(x, y, def.fcolor, def.bcolor, def.symbol);
		}
	}

	pkt_puts(state->player_x, state->player_y, PKT_COLOR_WHITE, PKT_COLOR_BLACK, "@");
	pkt_putc(state->cursor_x, state->cursor_y, PKT_COLOR_YELLOW, PKT_COLOR_BLACK, 'X');
}
