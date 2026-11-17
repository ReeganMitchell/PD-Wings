#include <jo/jo.h>

#include "game_state.h"
#include "level.h"

#define LEVEL_START_X   (540)
#define LEVEL_START_Y   (120)
#define BACKGROUND_MODEL_Z (400)
#define BACKGROUND_MODEL_SCALE (3)

static jo_camera g_background_camera;
static jo_3d_model *g_background_model;

bool level_load_assets(void)
{
    if (!jo_fs_cd("LVL"))
    {
        jo_core_error("Failed to enter LVL directory: %s", jo_get_last_error());
        return false;
    }
    if (!jo_fs_cd("TEST"))
    {
        jo_core_error("Failed to enter test level directory: %s",
                      jo_get_last_error());
        return false;
    }
    if (jo_sprite_add_image_pack("TEX", "TEST.TEX", JO_COLOR_Red) < 0)
    {
        jo_core_error("Failed to load test level texture pack: %s",
                      jo_get_last_error());
        return false;
    }
    if (!jo_map_load_from_file(
            GAME_WORLD_MAP_ID, 500, "MAP", "DEMO2.MAP"))
    {
        jo_core_error("Failed to load test level map: %s", jo_get_last_error());
        return false;
    }
    if (!jo_fs_cd("MOD"))
    {
        jo_core_error("Failed to enter test level model directory: %s",
                      jo_get_last_error());
        return false;
    }
    g_background_model = jo_3d_load_model("HOUSE.JOM");
    if (g_background_model == JO_NULL)
    {
        jo_core_error("Failed to load 3D background model HOUSE.JOM: %s",
                      jo_get_last_error());
        return false;
    }
    if (!jo_fs_cd(JO_PARENT_DIR) ||
        !jo_fs_cd(JO_PARENT_DIR) ||
        !jo_fs_cd(JO_PARENT_DIR))
    {
        jo_core_error("Failed to return to CD root: %s", jo_get_last_error());
        return false;
    }
    jo_3d_camera_init(&g_background_camera);
    return true;
}

void level_reset(level_state *level)
{
    level->map_x = LEVEL_START_X;
    level->map_y = LEVEL_START_Y;
}

void level_draw(const level_state *level)
{
    jo_map_draw(GAME_WORLD_MAP_ID, level->map_x, level->map_y);
}

void level_draw_background(const level_state *level)
{
    jo_fixed camera_offset_x;
    jo_fixed camera_offset_y;

    camera_offset_x = JO_FLOAT_TO_FIXED(
        (float)(level->map_x - LEVEL_START_X) / BACKGROUND_MODEL_SCALE);
    camera_offset_y = JO_FLOAT_TO_FIXED(
        (float)(level->map_y - LEVEL_START_Y) / BACKGROUND_MODEL_SCALE);
    g_background_camera.viewpoint_pos.x = camera_offset_x;
    g_background_camera.target_pos.x = camera_offset_x;
    g_background_camera.viewpoint_pos.y = camera_offset_y;
    g_background_camera.target_pos.y = camera_offset_y;

    jo_3d_push_matrix();
    jo_3d_camera_look_at(&g_background_camera);
    jo_3d_set_scale(
        BACKGROUND_MODEL_SCALE,
        BACKGROUND_MODEL_SCALE,
        BACKGROUND_MODEL_SCALE);
    jo_3d_translate_matrix(0, 20, BACKGROUND_MODEL_Z);
    jo_3d_rotate_matrix_x(-20);
    jo_3d_model_draw(g_background_model);
    jo_3d_pop_matrix();
}
