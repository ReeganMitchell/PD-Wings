#ifndef INPUT_H
#define INPUT_H

#include "game_state.h"

void input_capture_frame(void);
void input_end_simulation_step(void);
bool input_character_switch_requested(void);
bool input_update_character_selection(game_state *game);
void input_update_actions(game_state *game);
bool input_reset_requested(void);
void input_update_movement(game_state *game);

#endif
