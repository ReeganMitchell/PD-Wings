#include <jo/jo.h>

#include "game.h"
#include "game_state.h"
#include "input.h"
#include "level.h"
#include "player.h"
#include "profiler.h"

#define SIMULATION_RATE_HZ 60u
#define MAX_SIMULATION_STEPS_PER_FRAME 4u

static game_state g_game;
static unsigned int g_simulation_last_vblank;
static unsigned int g_simulation_accumulator;

static void game_update_simulation(void)
{
    unsigned int vblank_count = jo_core_get_vblank_count();
    unsigned int elapsed_vblanks = vblank_count - g_simulation_last_vblank;
    unsigned int refresh_rate = (unsigned int)jo_core_get_refresh_rate();
    unsigned int steps = 0;

    g_simulation_last_vblank = vblank_count;
    if (elapsed_vblanks > MAX_SIMULATION_STEPS_PER_FRAME)
        elapsed_vblanks = MAX_SIMULATION_STEPS_PER_FRAME;
    g_simulation_accumulator += elapsed_vblanks * SIMULATION_RATE_HZ;

    while (g_simulation_accumulator >= refresh_rate &&
           steps < MAX_SIMULATION_STEPS_PER_FRAME)
    {
        jo_profile_begin(JO_PROFILE_USER(0));
        input_update_character_selection(&g_game);
        if (input_reset_requested())
            game_reset();
        input_update_movement(&g_game);
        player_update(&g_game);
        player_update_animation(&g_game.player);
        jo_profile_end(JO_PROFILE_USER(0));

        input_end_simulation_step();
        g_simulation_accumulator -= refresh_rate;
        ++steps;
    }

    if (g_simulation_accumulator >= refresh_rate)
        g_simulation_accumulator %= refresh_rate;
}

bool game_init(void)
{
    if (!level_load_assets() || !player_load_assets(&g_game.player))
        return false;
    game_reset();
    profiler_init();
    g_simulation_last_vblank = jo_core_get_vblank_count();
    g_simulation_accumulator = 0;
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
    input_capture_frame();
    game_update_simulation();

    jo_profile_begin(JO_PROFILE_USER(1));
    level_draw(&g_game.level);
    player_draw(&g_game.player);
    jo_printf(0, 0, "LEVEL - %-12s",
              player_get_character_name(&g_game.player));
    jo_printf(0, 1, "D-PAD A:jump X:char B:perf START:reset");
    jo_printf(0, 2, "IDLE/RUN animations  max speed: %-12d",
              player_get_max_speed(&g_game.player));
    jo_printf(0, 3, "PAD:%-3s UP:%-8s RAW:%04X",
              jo_is_pad1_available() ? "yes" : "no",
              jo_is_pad1_key_pressed(JO_KEY_UP) ? "PRESSED" : "RELEASED",
              jo_get_raw_input_key_pressed(JO_INPUT_PORT1_FIRST_DEVICE));
    jo_profile_end(JO_PROFILE_USER(1));

    profiler_draw();
}
