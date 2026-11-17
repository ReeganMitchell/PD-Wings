#include <jo/jo.h>

#include "game_state.h"
#include "level.h"
#include "player.h"

#define PLAYER_START_X              (160)
#define PLAYER_START_Y              (70)
#define PLAYER_WIDTH_2              (GAME_PLAYER_WIDTH / 2)
#define MAP_TILE_BLOCK_ATTR         (1)
#define JUMP_PER_PIXEL_TOLERANCE    (20)

typedef struct
{
    const char *name;
    int max_speed;
} test_character;

static const test_character g_test_characters[] =
{
    {"Test character A", 6.0f},
    {"Test character B", 5.0f}
};

static const jo_tile g_walking_tiles[] =
{
    {0,   0, GAME_PLAYER_WIDTH, GAME_PLAYER_HEIGHT},
    {48,  0, GAME_PLAYER_WIDTH, GAME_PLAYER_HEIGHT},
    {96,  0, GAME_PLAYER_WIDTH, GAME_PLAYER_HEIGHT},
    {144, 0, GAME_PLAYER_WIDTH, GAME_PLAYER_HEIGHT},
    {192, 0, GAME_PLAYER_WIDTH, GAME_PLAYER_HEIGHT},
    {240, 0, GAME_PLAYER_WIDTH, GAME_PLAYER_HEIGHT},
    {288, 0, GAME_PLAYER_WIDTH, GAME_PLAYER_HEIGHT},
    {336, 0, GAME_PLAYER_WIDTH, GAME_PLAYER_HEIGHT}
};

static bool check_vertical_collision(game_state *game)
{
    player_state *player = &game->player;
    int distance;

    player->can_jump = false;
    if (player->physics.speed_y < 0.0f)
    {
        player->physics.is_in_air = true;
        return false;
    }
    distance = jo_map_per_pixel_vertical_collision(
        GAME_WORLD_MAP_ID,
        game->level.map_x + player->x + PLAYER_WIDTH_2,
        game->level.map_y + player->y + GAME_PLAYER_HEIGHT,
        JO_NULL);
    if (distance == JO_MAP_NO_COLLISION || distance > 0)
    {
        if (distance != JO_MAP_NO_COLLISION &&
            distance < JUMP_PER_PIXEL_TOLERANCE)
            player->can_jump = true;
        player->physics.is_in_air = true;
        return false;
    }
    if (distance < 0 && jo_is_float_equals_zero(player->physics.speed_y))
        player->y += distance;
    player->can_jump = true;
    player->physics.is_in_air = false;
    return true;
}

static bool check_horizontal_collision(game_state *game)
{
    player_state *player = &game->player;
    int next_x;
    int attribute;

    next_x = jo_physics_is_going_on_the_right(&player->physics) ?
        player->x + 4 :
        jo_physics_is_going_on_the_left(&player->physics) ?
        player->x - 4 :
        player->x;
    attribute = jo_map_hitbox_detection_custom_boundaries(
        GAME_WORLD_MAP_ID,
        game->level.map_x + next_x + PLAYER_WIDTH_2,
        game->level.map_y + player->y,
        4,
        20);
    return attribute == MAP_TILE_BLOCK_ATTR;
}

bool player_load_assets(player_state *player)
{
    int first_sprite_id;
    int i;

    if (!jo_fs_cd("CHAR") || !jo_fs_cd("TEST"))
    {
        jo_core_error("Failed to enter test character directory: %s",
                      jo_get_last_error());
        return false;
    }
    first_sprite_id = jo_sprite_add_bin_tileset(
        JO_CURRENT_DIR, "SNC.BIN", JO_COLOR_Green,
        g_walking_tiles, JO_TILE_COUNT(g_walking_tiles));
    if (first_sprite_id < 0)
    {
        jo_core_error("Failed to load test character spritesheet: %s",
                      jo_get_last_error());
        return false;
    }
    i = jo_create_sprite_anim(
        (unsigned short)first_sprite_id, JO_TILE_COUNT(g_walking_tiles), 4);
    if (i < 0)
    {
        jo_core_error("Failed to create player animation");
        return false;
    }
    player->walking_anim_id = i;
    if (!jo_fs_cd(JO_PARENT_DIR) || !jo_fs_cd(JO_PARENT_DIR))
    {
        jo_core_error("Failed to return to CD root: %s", jo_get_last_error());
        return false;
    }
    return true;
}

void player_reset(player_state *player)
{
    player->x = PLAYER_START_X;
    player->y = PLAYER_START_Y;
    player->facing_left = false;
    player->can_jump = true;
    player->wall_collision = false;
    player->horizontal_remainder = 0.0f;
    jo_physics_init_for_sonic(&player->physics);
    player->physics.max_speed = g_test_characters[player->character_index].max_speed;
    jo_reset_sprite_anim(player->walking_anim_id);
}

void player_select_next_character(player_state *player)
{
    player->character_index = (player->character_index + 1) %
        (sizeof(g_test_characters) / sizeof(g_test_characters[0]));
    player->physics.max_speed = g_test_characters[player->character_index].max_speed;
    if (player->physics.speed > player->physics.max_speed)
        player->physics.speed = player->physics.max_speed;
    else if (player->physics.speed < -player->physics.max_speed)
        player->physics.speed = -player->physics.max_speed;
}

const char *player_get_character_name(const player_state *player)
{
    return g_test_characters[player->character_index].name;
}

int player_get_max_speed(const player_state *player)
{
    return g_test_characters[player->character_index].max_speed;
}

void player_update(game_state *game)
{
    player_state *player = &game->player;
    int horizontal_delta;

    if (check_vertical_collision(game))
        player->physics.speed_y = 0.0f;
    else
    {
        jo_physics_apply_gravity(&player->physics);
        player->y += (int)player->physics.speed_y;
    }

    player->wall_collision = check_horizontal_collision(game);
    if (player->wall_collision)
    {
        player->physics.speed = 0.0f;
        player->horizontal_remainder = 0.0f;
    }
    else
    {
        player->horizontal_remainder += player->physics.speed;
        horizontal_delta = (int)player->horizontal_remainder;
        player->horizontal_remainder -= horizontal_delta;
        game->level.map_x += horizontal_delta;
    }

    if (player->y > 100)
    {
        game->level.map_y += player->y - 100;
        player->y = 100;
    }
    else if (player->y < 50)
    {
        game->level.map_y -= 50 - player->y;
        player->y = 50;
    }
}

void player_update_animation(player_state *player)
{
    int speed_step;

    if (jo_physics_is_standing(&player->physics))
    {
        if (!jo_is_sprite_anim_stopped(player->walking_anim_id))
            jo_reset_sprite_anim(player->walking_anim_id);
        return;
    }
    if (jo_is_sprite_anim_stopped(player->walking_anim_id))
        jo_start_sprite_anim_loop(player->walking_anim_id);
    speed_step = (int)JO_ABS(player->physics.speed);
    jo_set_sprite_anim_frame_rate(
        player->walking_anim_id, 9 - JO_MIN(speed_step, 6));
}

void player_draw(const player_state *player)
{
    int sprite_id = jo_get_anim_sprite(player->walking_anim_id);

    if (player->facing_left)
        jo_sprite_enable_horizontal_flip();
    jo_sprite_draw3D2(sprite_id, player->x, player->y, 450);
    if (player->facing_left)
        jo_sprite_disable_horizontal_flip();
}
