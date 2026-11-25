#include "player_character.h"

#define REI_MAX_SPEED (3)
#define REI_GROUND_ACCELERATION (JO_FIXED_1 / 4)
#define REI_AIR_ACCELERATION (JO_FIXED_1 * 3 / 32)
#define REI_FRICTION (JO_FIXED_1 * 2 / 25)

static const player_animation_definition g_animations[] =
{
    { PLAYER_ANIMATION_IDLE, "IDLE.BIN", 15, 48, 48, 6, true },
    { PLAYER_ANIMATION_RUN, "RUN.BIN", 8, 48, 48, 4, true },
    { PLAYER_ANIMATION_JUMP, "JUMP.BIN", 6, 48, 48, 4, true },
    { PLAYER_ANIMATION_WALL, "WALL.BIN", 6, 48, 48, 4, true },
    { PLAYER_ANIMATION_ROLL, "ROLL.BIN", 7, 48, 48, 3, true },
    { PLAYER_ANIMATION_SHOOT, "SHOOT.BIN", 10, 48, 48, 3, false },
    { PLAYER_ANIMATION_SHOOT_MOVE, "RSHOOT.BIN", 8, 48, 48, 3, false }
};

static const player_character_definition g_rei =
{
    "REI",
    "REI",
    g_animations,
    sizeof(g_animations) / sizeof(g_animations[0]),
    REI_MAX_SPEED,
    REI_GROUND_ACCELERATION,
    REI_AIR_ACCELERATION,
    REI_FRICTION,
    48,
    PLAYER_ANIMATION_ROLL,
    PLAYER_ANIMATION_SHOOT,
    PLAYER_ANIMATION_SHOOT_MOVE
};

const player_character_definition *player_rei_definition(void)
{
    return &g_rei;
}
