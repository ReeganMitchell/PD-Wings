#ifndef PLAYER_H
#define PLAYER_H

#include "game_state.h"

bool player_load_assets(player_state *player);
void player_reset(player_state *player);
bool player_select_next_character(player_state *player);
void player_set_down_move(player_state *player, bool active);
void player_start_attack(player_state *player);
const char *player_get_character_name(const player_state *player);
int player_get_max_speed(const player_state *player);
void player_update(game_state *game);
void player_update_animation(player_state *player);
void player_draw(const player_state *player);

#endif
