#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <jo/jo.h>

#define GAME_WORLD_MAP_ID       (0)
#define GAME_PLAYER_WIDTH       (48)

typedef struct
{
    int map_x;
    int map_y;
} level_state;

typedef struct
{
    jo_sidescroller_physics_params physics;
    float horizontal_remainder;
    int x;
    int y;
    int active_animation_id;
    int character_index;
    bool facing_left;
    bool can_jump;
    bool wall_collision;
} player_state;

typedef struct
{
    level_state level;
    player_state player;
} game_state;

#endif
