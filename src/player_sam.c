#include "player_character.h"

#define SAM_MAX_SPEED (4)
#define SAM_GROUND_ACCELERATION (JO_FIXED_1 / 4)
#define SAM_AIR_ACCELERATION (JO_FIXED_1 * 3 / 32)
#define SAM_FRICTION (JO_FIXED_1 * 2 / 25)

static const player_animation_definition g_animations[] =
{
    { PLAYER_ANIMATION_IDLE, "IDLE.BIN", 10, 48, 48, 6, true },
    { PLAYER_ANIMATION_RUN, "RUN.BIN", 8, 48, 48, 4, true },
    { PLAYER_ANIMATION_JUMP, "JUMP.BIN", 6, 48, 48, 4, true },
    { PLAYER_ANIMATION_WALL, "WALL.BIN", 6, 48, 48, 4, true },
    { PLAYER_ANIMATION_SLIDE, "SLIDE.BIN", 8, 48, 48, 3, true },
    { PLAYER_ANIMATION_ATTACK_GROUND, "STAB.BIN", 7, 96, 48, 3, false },
    { PLAYER_ANIMATION_ATTACK_AIR, "SPIN.BIN", 6, 48, 48, 3, false }
};

static const player_character_definition g_sam =
{
    "SAM",
    "SAM",
    g_animations,
    sizeof(g_animations) / sizeof(g_animations[0]),
    SAM_MAX_SPEED,
    SAM_GROUND_ACCELERATION,
    SAM_AIR_ACCELERATION,
    SAM_FRICTION,
    48,
    PLAYER_ANIMATION_SLIDE,
    PLAYER_ANIMATION_ATTACK_GROUND,
    PLAYER_ANIMATION_ATTACK_AIR
};

const player_character_definition *player_sam_definition(void)
{
    return &g_sam;
}
