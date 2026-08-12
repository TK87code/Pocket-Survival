#include "map.h"
#include "defines.h"
#include "data.h"
#include "noise.h"

void generate_map(struct game_state *s)
{
	for (int y = 0; y < MAP_ROWS; y++) {
		for (int x = 0; x < MAP_COLS; x++) {
			unsigned int o = OBJ_NONE;

			float e_base = value_noise_2d(x, y, s->seed, 200.f) * 0.8f;
			float e_detail = value_noise_2d(x, y, s->seed + 123, 15.0f) * 0.2f;
			float elevation = e_base + e_detail;

			float m_base = value_noise_2d(x, y, s->seed + 1234, 150.0f) * 0.8f;
			float m_detail = value_noise_2d(x, y, s->seed + 12345, 10.0f) * 0.2f;
			float moisture = m_base + m_detail;

			float dent = value_noise_2d(x, y, s->seed + 999, 20.0f);

			float richness = value_noise_2d(x, y, s->seed + 444, 100.0f);
			float o_dice = hash2d(x, y, s->seed);

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
			if ((o != OBJ_NONE) && ((object_defs[o].bitflags & FLAG_OBJ_WALKABLE) == 0))
				s->map[y][x].bitflags &= ~FLAG_CELL_WALKABLE;
		}
	}
}
