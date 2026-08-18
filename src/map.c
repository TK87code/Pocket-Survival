#include "map.h"
#include "defines.h"
#include "data.h"
#include "noise.h"

void generate_map(struct game_state *s)
{
	for (int wy = 0; wy < MAP_ROWS; wy++) {
		for (int wx = 0; wx < MAP_COLS; wx++) {
			unsigned int o = OBJ_NONE;

			float e_base = value_noise_2d(wx, wy, s->seed, 200.f) * 0.8f;
			float e_detail = value_noise_2d(wx, wy, s->seed + 123, 15.0f) * 0.2f;
			float elevation = e_base + e_detail;

			float m_base = value_noise_2d(wx, wy, s->seed + 1234, 150.0f) * 0.8f;
			float m_detail = value_noise_2d(wx, wy, s->seed + 12345, 10.0f) * 0.2f;
			float moisture = m_base + m_detail;

			float dent = value_noise_2d(wx, wy, s->seed + 999, 20.0f);

			float richness = value_noise_2d(wx, wy, s->seed + 444, 100.0f);
			float o_dice = hash2d(wx, wy, s->seed);

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

			s->map.terrains[GET_IDX(wx, wy)] = (uint8_t)t;
			s->map.objects[GET_IDX(wx, wy)] = (uint8_t)o;
			s->map.bitflags[GET_IDX(wx, wy)] = terrain_defs[t].bitflags;

			if (object_defs[o].bitflags & FLAG_OBJ_OBSTRACT)
				s->map.bitflags[GET_IDX(wx, wy)] |= FLAG_CELL_OBSTRACT;
		}
	}
}
