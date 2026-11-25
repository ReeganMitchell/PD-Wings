#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <jo/jo.h>

#define GAME_GRAPHICS_MAP_ID    (0)
#define GAME_COLLISION_MAP_ID   (1)
#define GAME_PLAYER_WIDTH       (48)

typedef struct
{
    int map_x;
    int map_y;
} level_state;

typedef struct
{
    jo_sidescroller_physics_params physics;
    jo_fixed horizontal_remainder;
    jo_fixed vertical_remainder;
    unsigned char animation_frame_skip;
    int x;
    int y;
    int active_animation_id;
    int character_index;
    bool down_move_active;
    bool attack_animation_active;
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
