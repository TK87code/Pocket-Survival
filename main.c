#include "pocket.h"

#define MAP_COL 80
#define MAP_ROW 24

enum terrain {
	TERRAIN_SEA,
	TERRAIN_SAND,
	TERRAIN_GRASS,
};

struct map_cell {
	enum terrain type;
	int is_walkable;
	enum pkt_color fcolor;
	enum pkt_color bcolor;
	char symbol;
};


struct game_state {
	struct map_cell map[MAP_ROW][MAP_COL];
};

void game_init(void *user_data) 
{
	struct game_state *state = (struct game_state *)user_data;

	for (int y = 0; y < MAP_ROW; y++) {
		for (int x = 0; x < MAP_COL; x++) {
			state->map[y][x].type = TERRAIN_SEA;
			state->map[y][x].fcolor = PKT_COLOR_BLUE;
			state->map[y][x].bcolor = PKT_COLOR_BLACK;
			state->map[y][x].symbol = '~';
		}
	}
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
			pkt_putc(x, y, state->map[y][x].fcolor, state->map[y][x].bcolor, state->map[y][x].symbol);
		}
	}
}

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

