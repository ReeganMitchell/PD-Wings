#ifndef PLAYER_CHARACTER_H
#define PLAYER_CHARACTER_H

#include <jo/jo.h>

typedef enum
{
    PLAYER_ANIMATION_IDLE,
    PLAYER_ANIMATION_RUN,
    PLAYER_ANIMATION_JUMP,
    PLAYER_ANIMATION_WALL,
    PLAYER_ANIMATION_ROLL,
    PLAYER_ANIMATION_SHOOT,
    PLAYER_ANIMATION_SHOOT_MOVE,
    PLAYER_ANIMATION_SLIDE,
    PLAYER_ANIMATION_ATTACK_GROUND,
    PLAYER_ANIMATION_ATTACK_AIR,
    PLAYER_ANIMATION_COUNT
} player_animation_slot;

typedef struct
{
    player_animation_slot slot;
    const char *filename;
    unsigned short frame_count;
    unsigned short frame_width;
    unsigned short frame_height;
    unsigned char frame_rate;
    bool loops;
} player_animation_definition;

typedef struct
{
    const char *name;
    const char *directory;
    const player_animation_definition *animations;
    unsigned int animation_count;
    int max_speed;
    jo_fixed ground_acceleration;
    jo_fixed air_acceleration;
    jo_fixed friction;
    int sprite_height;
    player_animation_slot down_move_animation;
    player_animation_slot grounded_attack;
    player_animation_slot aerial_attack;
} player_character_definition;

const player_character_definition *player_rei_definition(void);
const player_character_definition *player_sam_definition(void);

#endif
