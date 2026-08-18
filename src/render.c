#include "render.h"
#include "defines.h"
#include "data.h"
#include "ext/pkt_win.h"
#include <string.h>

#define COLOR_SELECTED 11
#define COMMAND_FC1 10 // fc for cmds
#define COMMAND_FC2 PKT_COLOR_WHITE // fc for cmd descriptions
#define COMMAND_BC 16 

struct cmd_line {
	const char *key;
	const char *desc;
	int ly;
	uint32_t mask_bit;
};

static void diversify_terrains(int wx, int wy, char *sym, unsigned int *fc, unsigned int t);
static unsigned int override_bc(struct game_state *s, int wx, int wy, unsigned int bc);

void draw_ingame_clock(struct game_state *s)
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

	pkt_win_printf_color(&s->win_status, 2, 0, (enum pkt_color)fc, (enum pkt_color)bc, PKT_ATTR_BOLD, 
			"Day %d - %s %02d:%02d", days, ampm, display_hour, mins);
}

void draw_speed_indicator(struct game_state *s)
{
	if (s->time_scale == 0.0f)
		pkt_win_puts_color(&s->win_log, 2, 0, 16, 6, PKT_ATTR_BOLD, "*PAUSED*");
	else
		pkt_win_printf_color(&s->win_log, 2, 0, 16, 6, PKT_ATTR_BOLD, "SPEED: x%.1f", s->time_scale);	
}

void draw_terrains(struct game_state *s)
{
	for (int ly = 0; ly < VIEWPORT_ROWS; ly++) {
		for (int lx = 0; lx < VIEWPORT_COLS; lx++) {
			int wx = s->cam_x + lx;
			int wy = s->cam_y + ly;

			unsigned int t = (unsigned int)s->map.terrains[GET_IDX(wx, wy)];

			char sym = terrain_defs[t].sym;
			unsigned int fc = terrain_defs[t].fc;
			unsigned int bc = override_bc(s, wx, wy, terrain_defs[t].bc);

			diversify_terrains(wx, wy, &sym, &fc, t);	
			pkt_win_putc_color(&s->win_map, lx, ly, (enum pkt_color)fc, bc, PKT_ATTR_NONE, sym);
		}
	}
}

void draw_objects(struct game_state *s)
{
	for (int ly = 0; ly < VIEWPORT_ROWS; ly++) {
		for (int lx = 0; lx < VIEWPORT_COLS; lx++) {
			int wx = s->cam_x + lx;
			int wy = s->cam_y + ly;

			unsigned int o = (unsigned int)s->map.objects[GET_IDX(wx, wy)];
			if (o == OBJ_NONE)
				continue;

			const struct object_def *od = &object_defs[o];
			unsigned int bc = override_bc(s, wx, wy, od->bc);

			if (od->sym_str != NULL)  
				pkt_win_puts_color(&s->win_map, lx, ly, 
						od->fc, bc, od->attr, od->sym_str); 
			else
				pkt_win_putc_color(&s->win_map, lx, ly, 
						od->fc, bc, od->attr, od->sym_char); 
		}
	}
}

void draw_overlays(struct game_state *s)
{
	for (int ly = 0; ly < VIEWPORT_ROWS; ly++) {
		for (int lx = 0; lx < VIEWPORT_COLS; lx++) {
			int wx = s->cam_x + lx;
			int wy = s->cam_y + ly;
			unsigned int f = s->map.bitflags[GET_IDX(wx, wy)];
			if (f == 0)
				continue;
			
			unsigned int bc = override_bc(s, wx, wy, 16);
			if (f & FLAG_CELL_MARKED)
				pkt_win_puts_color(&s->win_map, lx, ly, 1, bc, PKT_ATTR_BOLD, "¤");

			if (f & FLAG_CELL_PILE_AREA) 
				pkt_win_puts_color(&s->win_map, lx, ly, 102, bc, PKT_ATTR_BOLD, "░");	

		}
	}
}

void draw_entities(struct game_state *s)
{
	struct entity_data *e = &s->entities;
	for (int eid = 0; eid < e->count; eid++) {
		uint8_t etype = e->type[eid];
		unsigned int bc = override_bc(s, e->wx[eid], e->wy[eid], entity_defs[etype].bc);
		int lx = e->wx[eid] - s->cam_x;
		int ly = e->wy[eid] - s->cam_y;

		if (lx >= 0 && lx < VIEWPORT_COLS && ly >= 0 && ly < VIEWPORT_ROWS) {
			pkt_win_putc_color(&s->win_map, lx, ly,	
					entity_defs[etype].fc, bc, entity_defs[etype].attr, 
					entity_defs[etype].sym);
		}

		// Slash animation
		if (e->state[eid] == PLAYER_STATE_WORK) {
			int16_t tid = e->current_task_id[eid];	
			int is_task_fetch = (s->tasks.type[tid] == TASK_FETCH);
			int is_task_drop = (s->tasks.type[tid] == TASK_DROP);
			if ( is_task_fetch || is_task_drop)  
				continue;

			unsigned int idx = s->tasks.target_map_idx[e->current_task_id[eid]];
			int sx = IDX_TO_WX(idx) - s->cam_x;
			int sy = IDX_TO_WY(idx) - s->cam_y;

			if (sx >= 0 && sx < VIEWPORT_COLS && sy >= 0 && sy < VIEWPORT_ROWS) {
				if ((s->global_ticks / 30) % 2 == 0)  
					pkt_win_putc_color(&s->win_map, sx, sy, 220, 16, PKT_ATTR_BOLD, '/');
			}
		}
	}
}

inline void draw_items(struct game_state *s)
{
	for (int i = 0; i < s->items.count; i++) {
		int wx = IDX_TO_WX(s->items.map_idx[i]);
		int wy = IDX_TO_WY(s->items.map_idx[i]);
		int lx = wx - s->cam_x;
		int ly = wy - s->cam_y;

		if (lx >= 0 && lx < VIEWPORT_COLS && ly >= 0 && ly < VIEWPORT_ROWS) {
			const struct item_def *d = &item_defs[s->items.type[i]]; 
			unsigned int bc = override_bc(s, wx, wy, d->bc); 

			if (d->sym_str != NULL)
				pkt_win_puts_color(&s->win_map, lx, ly, d->fc, bc, d->attr, d->sym_str);
			else
				pkt_win_putc_color(&s->win_map, lx, ly, d->fc, bc, d->attr, d->sym_char);
		}
	}
}

void draw_cursor(struct game_state *s)
{
	int cursor_wx = s->cursor_lx + s->cam_x;
	int cursor_wy = s->cursor_ly + s->cam_y;
	//unsigned int fc = (is_in_selection(s, cursor_wx, cursor_wy)) ? 16 : PKT_COLOR_WHITE;
	unsigned int bc = override_bc(s, cursor_wx, cursor_wy, 16); 

	pkt_win_putc_color(&s->win_map, s->cursor_lx, s->cursor_ly, PKT_COLOR_RED, bc, PKT_ATTR_BOLD, 'X');
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

void draw_command_box(struct game_state *s)
{
	struct pkt_window *w = &s->win_command;

	pkt_win_fill(w, COMMAND_BC);
	pkt_win_box(w);
	pkt_win_puts(w, 2, 0, " COMMANDS ");

	struct cmd_line default_cmds [] = {
		{"d", ": Designations", 1, 0},
		{"p", ": Stockpile", 2, 0},
		{"SPACE", ": Pause game", 37, 0},
		{"Arrow UP/DOWN", ": Change speed", 38, 0},
	};

	struct cmd_line designate_cmds [] = {
		{"t", ": Chop down trees", 1, (1 << OBJ_TREE)},
		{"r", ": Mine rocks", 2, (1 << OBJ_ROCK)},
		{"g", ": Mown grass", 3, (1 << OBJ_GRASS)},
		{"Enter", ": Designate", 37, 0},
		{"ESC", ": Done", 38, 0},
	};

	struct cmd_line pile_cmds [] = {
		{"w", ": Wood", 1, (1 << ITEM_WOOD)},
		{"s", ": Stone", 2, (1 << ITEM_STONE)},
		{"Enter", ": Set pile area", 37, 0},
		{"ESC", ": Done", 38, 0},
	};

	struct cmd_line *cmd = NULL;
	int cnt = 0;

	if (s->mode == MODE_DEFAULT) {
		cmd = default_cmds;
		cnt = sizeof(default_cmds) / sizeof(default_cmds[0]);
	} else if (s->mode == MODE_DESIGNATE) {
		cmd = designate_cmds;
		cnt = sizeof(designate_cmds) / sizeof(default_cmds[0]);
	} else if (s->mode == MODE_PILE) {
		cmd = pile_cmds;
		cnt = sizeof(pile_cmds) / sizeof(pile_cmds[0]);
	}

	for (int i = 0; i < cnt; i++) {
		unsigned int desc_color = COMMAND_FC2;

		if (cmd[i].mask_bit != 0 && (s->drag_ctx.target_mask & cmd[i].mask_bit)) 
			desc_color = COLOR_SELECTED;

		pkt_win_puts_color(w, 2, cmd[i].ly, COMMAND_FC1, COMMAND_BC, PKT_ATTR_BOLD, cmd[i].key);

		int lx_offset = 2 + strlen(cmd[i].key);
		pkt_win_puts_color(w, lx_offset, cmd[i].ly, desc_color, COMMAND_BC, PKT_ATTR_BOLD, cmd[i].desc);
	}
}

static unsigned int override_bc(struct game_state *s, int wx, int wy, unsigned int bc)
{
	const struct dragging_context *dc = &s->drag_ctx;

	if ((dc->bitflags & FLAG_DRAG_ACTIVE) == 0)
		return bc;

	if (wx >= dc->min_wx && wx <= dc->max_wx && wy >= dc->min_wy && wy <= dc->max_wy)
		return (dc->bitflags & FLAG_DRAG_RESTRICTED) ? PKT_COLOR_RED : COLOR_SELECTED;

	return bc;
}
