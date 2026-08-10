#include "render.h"
#include "defines.h"
#include "data.h"

static void diversify_terrains(int wx, int wy, char *sym, unsigned int *fc, unsigned int t);

inline void draw_ingame_clock(struct game_state *s)
{
	int elapsed_mins = s->global_ticks / TICKS_PER_GAME_MINUTE;
	int abs_mins = elapsed_mins + (9 * 60); // New game starts from Day 1 9am;
	
	int mins = abs_mins % 60;
	int hours = (abs_mins / 60) % 24; 
	int days = (abs_mins / (60 * 24)) + 1;
	
	const char* ampm = (hours >= 12) ? "PM" : "AM";

	int display_hour = hours % 12;
	if (display_hour == 0)
		display_hour = 12;
	
	// Change backgroud color according to hours to represent daylight.
	unsigned int fc = 0;
	unsigned int bc = 0;
	if (hours <= 4) {
		fc = 15;
		bc = 235;
	} else if (hours <= 8) {
		fc = 1;
		bc = 215;
	} else if (hours <= 17) {
		fc = 16;
		bc = 227;
	} else if (hours <= 24) { 
		fc = 15;
		bc = 235;
	}

	pkt_win_printf_color(&s->win_status, 2, 0, (enum pkt_color)fc, (enum pkt_color)bc, PKT_ATTR_NONE, 
			"Day %d - %s %02d:%02d", days, ampm, display_hour, mins);
}

inline void draw_terrains(struct game_state *s, int x, int y)
{
	int wx = s->cam_x + x;
	int wy = s->cam_y + y;
	unsigned int t = s->map[wy][wx].terrain;
	char sym = terrain_defs[t].sym;
	unsigned int fc = terrain_defs[t].fc;

	diversify_terrains(wx, wy, &sym, &fc, t);	

	pkt_win_putc_color(&s->win_map, x, y, (enum pkt_color)fc, terrain_defs[t].bc, PKT_ATTR_NONE, sym);
}

inline void draw_objects(struct game_state *s, int x, int y)
{
	int wx = s->cam_x + x;
	int wy = s->cam_y + y;
	unsigned int o = s->map[wy][wx].object;

	if (o != OBJ_NONE) {
		const struct object_def *od = &object_defs[o];
		if (od->sym_str != NULL)  
			pkt_win_puts_color(&s->win_map, x, y, 
					od->fc, od->bc, od->attr, od->sym_str); 
		else
			pkt_win_putc_color(&s->win_map, x, y, 
					od->fc, od->bc, od->attr, od->sym_char); 
	}
}

inline void draw_entities(struct game_state *s)
{
	for (int i = 0; i < s->entity_count; i++) {
		struct entity *e = &s->entities[i];
		struct entity_def d = entity_defs[e->type];

		int ex = e->x - s->cam_x;
		int ey = e->y - s->cam_y;
		if (ex >= 0 && ex < VIEWPORT_COL && ey >= 0 && ey < VIEWPORT_ROW) 
			pkt_win_putc_color(&s->win_map, ex, ey,	d.fc, d.bc, PKT_ATTR_NONE, d.sym);

		// Slash animation
		if (e->state == ENT_STATE_WORK) {
			struct task *ts = &s->task_queue[e->current_task_id];	
			int sx = ts->target_x - s->cam_x;
			int sy = ts->target_y - s->cam_y;

			if (sx >= 0 && sx < VIEWPORT_COL && sy >= 0 && sy < VIEWPORT_ROW) {
				if ((s->global_ticks / 30) % 2 == 0) { 
					pkt_win_putc_color(&s->win_map, sx, sy, 220, 
							PKT_COLOR_BLACK, PKT_ATTR_NONE, '/');
				}
			}
		}
	}
}

inline void draw_items(struct game_state *s)
{
	for (int i = 0; i < s->dropped_item_count; i++) {
		struct item *itm = &s->items[i];
		int lx = itm->x - s->cam_x;
		int ly = itm->y - s->cam_y;

		if (lx >= 0 && lx < VIEWPORT_COL && ly >= 0 && ly < VIEWPORT_ROW) {
			const struct item_def *d = &item_defs[itm->type]; 
			if (d->sym_str != NULL)
				pkt_win_puts_color(&s->win_map, lx, ly, d->fc, d->bc, d->attr, d->sym_str);
			else
				pkt_win_putc_color(&s->win_map, lx, ly, d->fc, d->bc, d->attr, d->sym_char);
		}
	}
}

static void diversify_terrains(int wx, int wy, char *sym, unsigned int *fc, unsigned int t)
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

inline void draw_command_box(struct game_state *s)
{
	pkt_win_fill(&s->win_command, 16);
	pkt_win_box_color(&s->win_command, 16, 237, PKT_ATTR_NONE);
	pkt_win_puts(&s->win_command, 2, 0, " COMMANDS ");
}
