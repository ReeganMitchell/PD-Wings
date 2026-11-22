#include <jo/jo.h>

#include "game_state.h"
#include "level.h"

#define LEVEL_MAP_DEPTH (300)
#define LEVEL_SKY_COLOR JO_COLOR_RGB(135, 206, 235)

bool level_load_assets(void)
{
    jo_set_default_background_color(LEVEL_SKY_COLOR);
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
    jo_map_draw(GAME_GRAPHICS_MAP_ID, level->map_x, level->map_y);
}
