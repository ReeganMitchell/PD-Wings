#ifndef INPUT_H
#define INPUT_H

#include "game_state.h"

void input_update_character_selection(game_state *game);
bool input_reset_requested(void);
void input_update_movement(game_state *game);

#endif
