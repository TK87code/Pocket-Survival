#include "render.h"
#include "defines.h"
#include "data.h"
#include "ext/pkt_win.h"

#define COLOR_SELECTED 11
#define COMMAND_FC1 10 // fc for cmds
#define COMMAND_FC2 PKT_COLOR_WHITE // fc for cmd descriptions
#define COMMAND_BC 16 

struct cmd_text {
	const char *text;
	int lx, ly;
	enum pkt_color fc;
};

struct desig_target {
	const char *text;
};

static void diversify_terrains(int wx, int wy, char *sym, unsigned int *fc, unsigned int t);
static int is_in_selection(struct game_state *s, int wx, int wy);

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

void draw_terrains(struct game_state *s, int lx, int ly)
{
	int wx = s->cam_x + lx;
	int wy = s->cam_y + ly;
	struct map_cell c = s->map[wy][wx];

	char sym = terrain_defs[c.terrain].sym;
	unsigned int fc = terrain_defs[c.terrain].fc;
	unsigned int bc = (is_in_selection(s, wx, wy)) ? COLOR_SELECTED : terrain_defs[c.terrain].bc;

	diversify_terrains(wx, wy, &sym, &fc, c.terrain);	

	pkt_win_putc_color(&s->win_map, lx, ly, (enum pkt_color)fc, bc, PKT_ATTR_NONE, sym);
}

void draw_objects(struct game_state *s, int lx, int ly)
{
	int wx = s->cam_x + lx;
	int wy = s->cam_y + ly;
	struct map_cell c = s->map[wy][wx];

	if (c.object != OBJ_NONE) {
		const struct object_def *od = &object_defs[c.object];
		unsigned int bc = (is_in_selection(s, wx, wy)) ? COLOR_SELECTED : od->bc;

		if (od->sym_str != NULL)  
			pkt_win_puts_color(&s->win_map, lx, ly, 
					od->fc, bc, od->attr, od->sym_str); 
		else
			pkt_win_putc_color(&s->win_map, lx, ly, 
					od->fc, bc, od->attr, od->sym_char); 
	}
}

void draw_markers(struct game_state *s, int lx, int ly)
{
	int wx = s->cam_x + lx;
	int wy = s->cam_y + ly;
	struct map_cell c = s->map[wy][wx];
	if (!(c.bitflags & FLAG_CELL_MARKED))
		return; 

	unsigned int bc = (is_in_selection(s, wx, wy)) ? COLOR_SELECTED : 16;
	pkt_win_puts_color(&s->win_map, lx, ly, 1, bc, PKT_ATTR_BOLD, "¤");
}

void draw_entities(struct game_state *s)
{
	for (int i = 0; i < s->entity_count; i++) {
		struct entity *e = &s->entities[i];
		struct entity_def d = entity_defs[e->type];
		unsigned int bc = (is_in_selection(s, e->wx, e->wy)) ? COLOR_SELECTED : d.bc;

		int ex = e->wx - s->cam_x;
		int ey = e->wy - s->cam_y;
		if (ex >= 0 && ex < VIEWPORT_COL && ey >= 0 && ey < VIEWPORT_ROW) 
			pkt_win_putc_color(&s->win_map, ex, ey,	d.fc, bc, PKT_ATTR_NONE, d.sym);

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
			unsigned int bc = (is_in_selection(s, itm->x, itm->y)) ? COLOR_SELECTED : d->bc; 

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
	unsigned int fc = (is_in_selection(s, cursor_wx, cursor_wy)) ? 16 : PKT_COLOR_WHITE;
	unsigned int bc = (is_in_selection(s, cursor_wx, cursor_wy)) ? COLOR_SELECTED : 16; 

	pkt_win_putc_color(&s->win_map, s->cursor_lx, s->cursor_ly, fc, bc, PKT_ATTR_BOLD, 'X');
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

	struct cmd_text default_cmds [] = {
		{"d", 2, 1, COMMAND_FC1}, {": Designations", 3, 1, COMMAND_FC2},
		{"SPACE", 2, 37, COMMAND_FC1}, {": Pause game", 7, 37, COMMAND_FC2},
		{"Arrow UP/DOWN", 2, 38, COMMAND_FC1}, {": Change speed", 15, 38, COMMAND_FC2},
	};

	struct cmd_text designate_cmds [] = {
		{"t", 2, 3, COMMAND_FC1}, {": Chop down trees", 3, 3, COMMAND_FC2},
		{"r", 2, 4, COMMAND_FC1}, {": Mine rocks", 3, 4, COMMAND_FC2},
		{"g", 2, 5, COMMAND_FC1}, {": Mown grass", 3, 5, COMMAND_FC2},
		{"a", 2, 6, COMMAND_FC1}, {": Clear the area", 3, 6, COMMAND_FC2},
		{"Enter", 2, 37, COMMAND_FC1}, {": Designate", 7, 37, COMMAND_FC2},
		{"ESC", 19, 37, COMMAND_FC1}, {": Done", 22, 37, COMMAND_FC2},
	};

	struct desig_target dt[] = {
		[OBJ_ALL] = {"All"},
		[OBJ_TREE] = {"Tree"},
		[OBJ_ROCK] = {"Rock"},
		[OBJ_GRASS] = {"Grass"},
	};

	struct cmd_text *cmd = NULL;
	int cnt = 0;

	if (s->mode == MODE_DEFAULT) {
		cmd = default_cmds;
		cnt = sizeof(default_cmds) / sizeof(default_cmds[0]);
	} else if (s->mode == MODE_DESIGNATE) {
		pkt_win_printf_color(w, 2, 1, COMMAND_FC2, COMMAND_BC, PKT_ATTR_BOLD, "TARGET-> [%s]", dt[s->desig_ctx.target]); 
		cmd = designate_cmds;
		cnt = sizeof(designate_cmds) / sizeof(default_cmds[0]);
	}

	for (int i = 0; i < cnt; i++) 
		pkt_win_puts_color(w, cmd[i].lx, cmd[i].ly, cmd[i].fc, 
				(enum pkt_color)COMMAND_BC, PKT_ATTR_BOLD, cmd[i].text); 
}

static int is_in_selection(struct game_state *s, int wx, int wy)
{
	if (s->mode != MODE_DESIGNATE || s->desig_ctx.is_dragging == 0)
		return 0;

	int cursor_wx = s->cursor_lx + s->cam_x;
	int cursor_wy = s->cursor_ly + s->cam_y;
	int start_x = s->desig_ctx.start_wx;
	int start_y = s->desig_ctx.start_wy;
	
	int min_x = (start_x < cursor_wx) ? start_x : cursor_wx;
	int max_x = (start_x > cursor_wx) ? start_x : cursor_wx;
	int min_y = (start_y < cursor_wy) ? start_y : cursor_wy;
	int max_y = (start_y > cursor_wy) ? start_y : cursor_wy;

	if (wx >= min_x && wx <= max_x && wy >= min_y && wy <= max_y)
		return 1;

	return 0;
}
