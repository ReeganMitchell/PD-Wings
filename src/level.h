#ifndef LEVEL_H
#define LEVEL_H

#include "game_state.h"

bool level_load_assets(void);
void level_reset(level_state *level);
void level_draw(const level_state *level);

#endif
