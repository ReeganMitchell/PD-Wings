#include <jo/jo.h>

#include "game.h"
#include "game_state.h"
#include "input.h"
#include "level.h"
#include "player.h"

static game_state g_game;

bool game_init(void)
{
    if (!level_load_assets() || !player_load_assets(&g_game.player))
        return false;
    game_reset();
    jo_audio_play_cd_track(2, 2, true);
    return true;
}

void game_reset(void)
{
    level_reset(&g_game.level);
    player_reset(&g_game.player);
}

void game_loop(void)
{
    input_update_character_selection(&g_game);
    if (input_reset_requested())
        game_reset();
    input_update_movement(&g_game);
    player_update(&g_game);
    player_update_animation(&g_game.player);

    jo_clear_screen();
    level_draw_background(&g_game.level);
    level_draw(&g_game.level);
    player_draw(&g_game.player);
    jo_printf(0, 0, "TEST LEVEL - %s",
              player_get_character_name(&g_game.player));
    jo_printf(0, 1, "D-PAD: move  A: jump  X: switch  START: reset");
    jo_printf(0, 2, "Same test sprite; max speed: %d",
              player_get_max_speed(&g_game.player));
    jo_printf(0, 3, "Ground: %s  Wall: %s",
              g_game.player.physics.is_in_air ? "no" : "yes",
              g_game.player.wall_collision ? "yes" : "no");
}
