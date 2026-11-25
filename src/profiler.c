#include <jo/jo.h>

#include "profiler.h"

#define PROFILER_MEMORY_SAMPLE_FRAMES 50u
#define PROFILER_FIRST_LINE 4
#define PROFILER_FOOTER_LINE 27

static bool g_profiler_visible;
static int g_memory_usage;
static int g_memory_fragmentation;
static unsigned int g_last_memory_sample;

static void clear_profiler_output(void)
{
    int line;

    for (line = PROFILER_FIRST_LINE; line < PROFILER_FOOTER_LINE; ++line)
        jo_printf(0, line, "%40s", "");
    jo_printf(0, PROFILER_FOOTER_LINE, "%40s", "");
}

void profiler_init(void)
{
    g_profiler_visible = false;
    g_memory_usage = 0;
    g_memory_fragmentation = 0;
    g_last_memory_sample = jo_core_get_vblank_count() -
                           PROFILER_MEMORY_SAMPLE_FRAMES;
    jo_profile_set_name(JO_PROFILE_USER(0), "update");
    jo_profile_set_name(JO_PROFILE_USER(1), "draw");
}

void profiler_update(void)
{
    if (!jo_is_pad1_available() ||
        !jo_is_pad1_key_down(JO_KEY_Y))
        return;

    g_profiler_visible = !g_profiler_visible;
    if (g_profiler_visible)
        g_last_memory_sample = jo_core_get_vblank_count() -
                               PROFILER_MEMORY_SAMPLE_FRAMES;
    else
        clear_profiler_output();
}

void profiler_draw(void)
{
    unsigned int vblank_count;

    if (!g_profiler_visible)
        return;

    vblank_count = jo_core_get_vblank_count();
    if (vblank_count - g_last_memory_sample >=
        PROFILER_MEMORY_SAMPLE_FRAMES)
    {
        g_memory_usage = jo_memory_usage_percent();
        g_memory_fragmentation = jo_memory_fragmentation();
        g_last_memory_sample = vblank_count;
    }

    jo_profile_print(0, PROFILER_FIRST_LINE);
    jo_printf(0, PROFILER_FOOTER_LINE,
              "%d FPS late:%d mem:%d%% frag:%d Y:hide",
              jo_core_get_fps(),
              jo_core_get_late_frame_count(),
              g_memory_usage,
              g_memory_fragmentation);
}
