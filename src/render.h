#ifndef RENDER_H
#define RENDER_H

#include "defines.h"

void draw_ingame_clock(struct game_state *s);
void draw_terrains(struct game_state *s, int x, int y);
void draw_objects(struct game_state *s, int x, int y);
void draw_markers(struct game_state *s, int x, int y);
void draw_entities(struct game_state *s);
void draw_items(struct game_state *s);
void draw_command_box(struct game_state *s);
void draw_speed_indicator(struct game_state *s);

#endif //RENDER_H
