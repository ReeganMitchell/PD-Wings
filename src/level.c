#include <jo/jo.h>

#include "game_state.h"
#include "level.h"

#define LEVEL_MAP_DEPTH (300)
#define LEVEL_SKY_COLOR JO_COLOR_RGB(135, 206, 235)
#define LEVEL_MAX_MODELS (32)
#define LEVEL_MODEL_VIEW_DEPTH (301)
#define LEVEL_MODEL_SCALE (3)

typedef struct
{
    jo_3d_model *model;
    int x;
    int y;
} level_model;

static jo_camera g_level_camera;
static level_model g_level_models[LEVEL_MAX_MODELS];
static int g_level_model_count;

static bool level_load_models(void)
{
    char *data;
    char *line;
    int length;
    int i;

    g_level_model_count = 0;
    if (!jo_fs_cd("MOD"))
    {
        jo_core_error("Failed to enter Z1A1 MOD directory: %s",
                      jo_get_last_error());
        return false;
    }
    data = jo_fs_read_file("Z1A1.MODMAP", &length);
    if (data == JO_NULL)
    {
        jo_core_error("Failed to load Z1A1.MODMAP: %s",
                      jo_get_last_error());
        jo_fs_cd(JO_PARENT_DIR);
        return false;
    }
    data[length] = '\0';

    line = data;
    for (i = 0; i < LEVEL_MAX_MODELS && *line != '\0'; ++i)
    {
        char filename[13];
        int x;
        int y;
        char *cursor = line;
        char *tab;
        int filename_length;
        char *newline;
        int sign;

        while (*cursor == ' ' || *cursor == '\t')
            ++cursor;
        tab = cursor;
        while (*tab != '\0' && *tab != '\t')
            ++tab;
        filename_length = tab - cursor;
        if (filename_length > 0 && filename_length < (int)sizeof(filename)
            && *tab == '\t')
        {
            jo_memcpy(filename, cursor, (unsigned int)filename_length);
            filename[filename_length] = '\0';
            cursor = tab + 1;
            x = 0;
            sign = 1;
            if (*cursor == '-')
            {
                sign = -1;
                ++cursor;
            }
            while (*cursor >= '0' && *cursor <= '9')
            {
                x = x * 10 + (*cursor - '0');
                ++cursor;
            }
            x *= sign;
            while (*cursor == ' ' || *cursor == '\t')
                ++cursor;
            y = 0;
            sign = 1;
            if (*cursor == '-')
            {
                sign = -1;
                ++cursor;
            }
            while (*cursor >= '0' && *cursor <= '9')
            {
                y = y * 10 + (*cursor - '0');
                ++cursor;
            }
            y *= sign;

            if (g_level_model_count < LEVEL_MAX_MODELS)
            {
                jo_3d_model *model = jo_3d_load_model(filename);
                if (model == JO_NULL)
                {
                    jo_core_error("Failed to load 3D model %s: %s",
                                  filename, jo_get_last_error());
                    jo_fs_cd(JO_PARENT_DIR);
                    return false;
                }
                g_level_models[g_level_model_count].model = model;
                g_level_models[g_level_model_count].x = x;
                g_level_models[g_level_model_count].y = y;
                ++g_level_model_count;
            }
        }
        newline = line;
        while (*newline != '\0' && *newline != '\n')
            ++newline;
        if (*newline == '\0')
            break;
        line = newline + 1;
    }

    if (!jo_fs_cd(JO_PARENT_DIR))
    {
        jo_core_error("Failed to return from Z1A1 MOD directory: %s",
                      jo_get_last_error());
        return false;
    }
    return true;
}

bool level_load_assets(void)
{
    jo_set_default_background_color(LEVEL_SKY_COLOR);
    jo_3d_camera_init(&g_level_camera);
    jo_3d_camera_set_viewpoint(&g_level_camera, 0, 0, -150);
    jo_3d_camera_set_target(&g_level_camera, 0, 0, 0);
    if (!jo_fs_cd("LVL"))
    {
        jo_core_error("Failed to enter LVL directory: %s", jo_get_last_error());
        return false;
    }
    if (!jo_fs_cd("ZONE1"))
    {
        jo_core_error("Failed to enter ZONE1 directory: %s",
                      jo_get_last_error());
        return false;
    }
    if (jo_sprite_add_image_pack("TEX", "Z1A1.TEX", JO_COLOR_Green) < 0)
    {
        jo_core_error("Failed to load Z1A1 texture pack: %s",
                      jo_get_last_error());
        return false;
    }
    if (!jo_map_load_from_file(
            GAME_GRAPHICS_MAP_ID, LEVEL_MAP_DEPTH, "MAP", "Z1A1G.MAP"))
    {
        jo_core_error("Failed to load Z1A1 graphics map: %s", jo_get_last_error());
        return false;
    }
    if (jo_sprite_add_image_pack("TEX", "Z1C.TEX", JO_COLOR_Green) < 0)
    {
        jo_core_error("Failed to load Z1A1 collision texture pack: %s",
                      jo_get_last_error());
        return false;
    }
    if (!jo_map_load_from_file(
            GAME_COLLISION_MAP_ID, 0, "MAP", "Z1A1C.MAP"))
    {
        jo_core_error("Failed to load Z1A1 collision map: %s", jo_get_last_error());
        return false;
    }
    if (!level_load_models())
        return false;
    if (!jo_fs_cd(JO_PARENT_DIR) ||
        !jo_fs_cd(JO_PARENT_DIR))
    {
        jo_core_error("Failed to return to CD root: %s", jo_get_last_error());
        return false;
    }
    return true;
}

void level_reset(level_state *level)
{
    level->map_x = 0;
    level->map_y = 0;
}

void level_draw(const level_state *level)
{
    int i;

    jo_map_draw(GAME_GRAPHICS_MAP_ID, level->map_x, level->map_y);
    jo_3d_camera_look_at(&g_level_camera);
    jo_3d_push_matrix();
    {
        for (i = 0; i < g_level_model_count; ++i)
        {
            jo_fixed projection;
            jo_fixed required_x;
            jo_fixed required_y;
            int screen_x;
            int screen_y;
            int depth;

            jo_3d_push_matrix();
            {
                jo_3d_set_scale(
                    LEVEL_MODEL_SCALE,
                    LEVEL_MODEL_SCALE,
                    LEVEL_MODEL_SCALE);
                projection = jo_3d_get_projection_distance();
                depth = LEVEL_MODEL_VIEW_DEPTH;

                screen_x = g_level_models[i].x - level->map_x -
                           (jo_core_get_screen_width() / 2);
                screen_y = g_level_models[i].y - level->map_y -
                           (jo_core_get_screen_height() / 2);
                required_x = jo_fixed_mult(
                    jo_int2fixed(screen_x),
                    jo_fixed_div(jo_int2fixed(depth), projection));
                required_y = jo_fixed_mult(
                    jo_int2fixed(screen_y),
                    jo_fixed_div(jo_int2fixed(depth), projection));
                jo_3d_translate_matrix_fixed(
                    required_x,
                    required_y,
                    jo_int2fixed(depth));
                jo_3d_model_draw(g_level_models[i].model);
            }
            jo_3d_pop_matrix();
        }
    }
    jo_3d_pop_matrix();
}
