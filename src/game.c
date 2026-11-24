#include <jo/jo.h>

#include "game.h"
#include "game_state.h"
#include "input.h"
#include "level.h"
#include "player.h"
#include "profiler.h"

static game_state g_game;

bool game_init(void)
{
    if (!level_load_assets() || !player_load_assets(&g_game.player))
        return false;
    game_reset();
    profiler_init();
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
    profiler_update();

    jo_profile_begin(JO_PROFILE_USER(0));
    input_update_character_selection(&g_game);
    if (input_reset_requested())
        game_reset();
    input_update_movement(&g_game);
    player_update(&g_game);
    player_update_animation(&g_game.player);
    jo_profile_end(JO_PROFILE_USER(0));

    jo_profile_begin(JO_PROFILE_USER(1));
    level_draw(&g_game.level);
    player_draw(&g_game.player);
    jo_printf(0, 0, "LEVEL - %-12s",
              player_get_character_name(&g_game.player));
    jo_printf(0, 1, "D-PAD A:jump X:char B:perf START:reset");
    jo_printf(0, 2, "IDLE/RUN animations  max speed: %-12d",
              player_get_max_speed(&g_game.player));
    jo_printf(0, 3, "Ground: %-3s  Wall: %-3s",
              g_game.player.physics.is_in_air ? "no" : "yes",
              g_game.player.wall_collision ? "yes" : "no");
    jo_profile_end(JO_PROFILE_USER(1));

    profiler_draw();
}
