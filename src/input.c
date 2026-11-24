#include <jo/jo.h>

#include "input.h"
#include "player.h"

static bool g_left_held;
static bool g_right_held;
static bool g_jump_pressed;
static bool g_character_switch_pressed;
static bool g_reset_pressed;

void input_capture_frame(void)
{
    if (!jo_is_pad1_available())
    {
        g_left_held = false;
        g_right_held = false;
        g_jump_pressed = false;
        g_character_switch_pressed = false;
        g_reset_pressed = false;
        return;
    }

    g_left_held = jo_is_pad1_key_pressed(JO_KEY_LEFT);
    g_right_held = jo_is_pad1_key_pressed(JO_KEY_RIGHT);
    g_jump_pressed |= jo_is_pad1_key_down(JO_KEY_A);
    g_character_switch_pressed |= jo_is_pad1_key_down(JO_KEY_X);
    g_reset_pressed |= jo_is_pad1_key_down(JO_KEY_START);
}

void input_end_simulation_step(void)
{
    g_jump_pressed = false;
    g_character_switch_pressed = false;
    g_reset_pressed = false;
}

void input_update_character_selection(game_state *game)
{
    if (g_character_switch_pressed)
        player_select_next_character(&game->player);
}

bool input_reset_requested(void)
{
    return g_reset_pressed;
}

void input_update_movement(game_state *game)
{
    player_state *player = &game->player;

    if (g_left_held)
    {
        player->facing_left = true;
        if (!player->physics.is_in_air &&
            (jo_physics_is_going_on_the_right(&player->physics) ||
             jo_physics_should_brake(&player->physics)))
            jo_physics_decelerate_left(&player->physics);
        else
            jo_physics_accelerate_left(&player->physics);
    }
    else if (g_right_held)
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

    if (player->can_jump && g_jump_pressed)
    {
        player->can_jump = false;
        jo_physics_jump(&player->physics);
    }
}
