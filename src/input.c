#include <jo/jo.h>

#include "input.h"
#include "player.h"

void input_update_character_selection(game_state *game)
{
    if (jo_is_pad1_available() && jo_is_pad1_key_down(JO_KEY_X))
        player_select_next_character(&game->player);
}

bool input_reset_requested(void)
{
    return jo_is_pad1_available() && jo_is_pad1_key_down(JO_KEY_START);
}

void input_update_movement(game_state *game)
{
    player_state *player = &game->player;

    if (!jo_is_pad1_available())
        return;

    if (jo_is_pad1_key_pressed(JO_KEY_LEFT))
    {
        player->facing_left = true;
        if (!player->physics.is_in_air &&
            (jo_physics_is_going_on_the_right(&player->physics) ||
             jo_physics_should_brake(&player->physics)))
            jo_physics_decelerate_left(&player->physics);
        else
            jo_physics_accelerate_left(&player->physics);
    }
    else if (jo_is_pad1_key_pressed(JO_KEY_RIGHT))
    {
        player->facing_left = false;
        if (!player->physics.is_in_air &&
            (jo_physics_is_going_on_the_left(&player->physics) ||
             jo_physics_should_brake(&player->physics)))
            jo_physics_decelerate_right(&player->physics);
        else
            jo_physics_accelerate_right(&player->physics);
    }
    else if (!player->physics.is_in_air)
        jo_physics_apply_friction(&player->physics);

    if (player->can_jump && jo_is_pad1_key_down(JO_KEY_A))
    {
        player->can_jump = false;
        jo_physics_jump(&player->physics);
    }
}
