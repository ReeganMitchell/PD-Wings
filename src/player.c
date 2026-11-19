#include <jo/jo.h>

#include "game_state.h"
#include "level.h"
#include "player.h"

#define PLAYER_START_X              (160)
#define PLAYER_START_Y              (70)
#define PLAYER_WIDTH_2              (GAME_PLAYER_WIDTH / 2)
#define PLAYER_COLLISION_LEFT       (14)
#define PLAYER_COLLISION_RIGHT      (35)
#define PLAYER_COLLISION_TOP        (10)
#define PLAYER_COLLISION_BOTTOM     (40)
#define PLAYER_COLLISION_LOOKAHEAD  (4)
#define MAP_TILE_BLOCK_ATTR         (1)
#define JUMP_PER_PIXEL_TOLERANCE    (20)
#define PLAYER_MAX_ANIMATION_FRAMES (15)
#define PLAYER_IDLE_FRAME_RATE      (6)

typedef struct
{
    const char *name;
    const char *directory;
    const char *idle_filename;
    const char *run_filename;
    unsigned short idle_frame_count;
    unsigned short run_frame_count;
    int max_speed;
    int sprite_height;
    bool shares_animation_sheet;
    int idle_anim_id;
    int run_anim_id;
} player_character;

static player_character g_characters[] =
{
    {"REI", "REI", "IDLE.BIN", "RUN.BIN", 15, 8, 4, 48, false, -1, -1},
    {"SAM", "SAM", "IDLE.BIN", "RUN.BIN", 10, 8, 5, 48, false, -1, -1}
};

static void create_frame_tileset(jo_tile *tiles, int frame_count, int height)
{
    int i;

    for (i = 0; i < frame_count; ++i)
    {
        tiles[i].x = i * GAME_PLAYER_WIDTH;
        tiles[i].y = 0;
        tiles[i].width = GAME_PLAYER_WIDTH;
        tiles[i].height = height;
    }
}

static int load_character_sheet(const char *filename, int frame_count,
                                int height)
{
    jo_tile tiles[PLAYER_MAX_ANIMATION_FRAMES];

    create_frame_tileset(tiles, frame_count, height);
    return jo_sprite_add_bin_tileset(
        JO_CURRENT_DIR, filename, JO_COLOR_Green, tiles,
        (unsigned int)frame_count);
}

static bool load_character_animations(player_character *character)
{
    int idle_sprite_id;
    int run_sprite_id;

    if (!jo_fs_cd(character->directory))
    {
        jo_core_error("Failed to enter %s character directory: %s",
                      character->name, jo_get_last_error());
        return false;
    }
    if (character->shares_animation_sheet)
    {
        run_sprite_id = load_character_sheet(
            character->run_filename, character->run_frame_count,
            character->sprite_height);
        idle_sprite_id = run_sprite_id;
    }
    else
    {
        idle_sprite_id = load_character_sheet(
            character->idle_filename, character->idle_frame_count,
            character->sprite_height);
        run_sprite_id = load_character_sheet(
            character->run_filename, character->run_frame_count,
            character->sprite_height);
    }
    if (idle_sprite_id < 0 || run_sprite_id < 0)
    {
        jo_core_error("Failed to load %s character spritesheet: %s",
                      character->name, jo_get_last_error());
        if (!jo_fs_cd(JO_PARENT_DIR))
            jo_core_error("Failed to return to the character directory: %s",
                          jo_get_last_error());
        return false;
    }
    character->idle_anim_id = jo_create_sprite_anim(
        (unsigned short)idle_sprite_id, character->idle_frame_count,
        PLAYER_IDLE_FRAME_RATE);
    character->run_anim_id = jo_create_sprite_anim(
        (unsigned short)run_sprite_id, character->run_frame_count, 4);
    if (character->idle_anim_id < 0 || character->run_anim_id < 0)
    {
        jo_core_error("Failed to create %s character animations",
                      character->name);
        if (!jo_fs_cd(JO_PARENT_DIR))
            jo_core_error("Failed to return to the character directory: %s",
                          jo_get_last_error());
        return false;
    }
    if (!jo_fs_cd(JO_PARENT_DIR))
    {
        jo_core_error("Failed to return from %s character directory: %s",
                      character->name, jo_get_last_error());
        return false;
    }
    return true;
}

static player_character *current_character(const player_state *player)
{
    return &g_characters[player->character_index];
}

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
        game->level.map_y + player->y + PLAYER_COLLISION_BOTTOM,
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
    int probe_x;
    int attribute;

    if (jo_physics_is_going_on_the_right(&player->physics))
    {
        next_x = player->x + PLAYER_COLLISION_LOOKAHEAD;
        probe_x = next_x + PLAYER_COLLISION_RIGHT;
    }
    else if (jo_physics_is_going_on_the_left(&player->physics))
    {
        next_x = player->x - PLAYER_COLLISION_LOOKAHEAD;
        probe_x = next_x + PLAYER_COLLISION_LEFT;
    }
    else
    {
        next_x = player->x;
        probe_x = next_x + PLAYER_COLLISION_RIGHT;
    }
    attribute = jo_map_hitbox_detection_custom_boundaries(
        GAME_WORLD_MAP_ID,
        game->level.map_x + probe_x,
        game->level.map_y + player->y + PLAYER_COLLISION_TOP,
        1,
        PLAYER_COLLISION_BOTTOM - PLAYER_COLLISION_TOP);
    return attribute == MAP_TILE_BLOCK_ATTR;
}

bool player_load_assets(player_state *player)
{
    int i;

    if (!jo_fs_cd("CHAR"))
    {
        jo_core_error("Failed to enter character directory: %s",
                      jo_get_last_error());
        return false;
    }
    for (i = 0; i < (int)(sizeof(g_characters) / sizeof(g_characters[0])); ++i)
    {
        if (!load_character_animations(&g_characters[i]))
        {
            if (!jo_fs_cd(JO_PARENT_DIR))
                jo_core_error("Failed to return to CD root: %s",
                              jo_get_last_error());
            return false;
        }
    }
    if (!jo_fs_cd(JO_PARENT_DIR))
    {
        jo_core_error("Failed to return to CD root: %s", jo_get_last_error());
        return false;
    }
    player->character_index = 0;
    player->active_animation_id = -1;
    return true;
}

void player_reset(player_state *player)
{
    int i;

    player->x = PLAYER_START_X;
    player->y = PLAYER_START_Y;
    player->facing_left = false;
    player->can_jump = true;
    player->wall_collision = false;
    player->horizontal_remainder = 0.0f;
    player->active_animation_id = -1;
    for (i = 0; i < (int)(sizeof(g_characters) / sizeof(g_characters[0])); ++i)
    {
        jo_reset_sprite_anim(g_characters[i].idle_anim_id);
        jo_reset_sprite_anim(g_characters[i].run_anim_id);
    }
    jo_physics_init_for_sonic(&player->physics);
    player->physics.max_speed = current_character(player)->max_speed;
}

void player_select_next_character(player_state *player)
{
    player_character *previous_character = current_character(player);
    unsigned int character_count =
        sizeof(g_characters) / sizeof(g_characters[0]);

    player->character_index = (player->character_index + 1) %
        character_count;
    player->y += previous_character->sprite_height -
        current_character(player)->sprite_height;
    player->physics.max_speed = current_character(player)->max_speed;
    if (player->physics.speed > player->physics.max_speed)
        player->physics.speed = player->physics.max_speed;
    else if (player->physics.speed < -player->physics.max_speed)
        player->physics.speed = -player->physics.max_speed;
}

const char *player_get_character_name(const player_state *player)
{
    return current_character(player)->name;
}

int player_get_max_speed(const player_state *player)
{
    return current_character(player)->max_speed;
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
    player_character *character = current_character(player);
    int animation_id = jo_physics_is_standing(&player->physics) ?
        character->idle_anim_id : character->run_anim_id;
    int speed_step;

    if (animation_id != player->active_animation_id)
    {
        if (player->active_animation_id >= 0)
            jo_reset_sprite_anim(player->active_animation_id);
        jo_reset_sprite_anim(animation_id);
        player->active_animation_id = animation_id;
    }
    if (jo_is_sprite_anim_stopped(animation_id))
        jo_start_sprite_anim_loop(animation_id);
    if (animation_id == character->idle_anim_id)
        return;
    speed_step = (int)JO_ABS(player->physics.speed);
    jo_set_sprite_anim_frame_rate(
        animation_id, 9 - JO_MIN(speed_step, 6));
}

void player_draw(const player_state *player)
{
    int sprite_id = jo_get_anim_sprite(player->active_animation_id);

    if (player->facing_left)
        jo_sprite_enable_horizontal_flip();
    jo_sprite_draw3D2(sprite_id, player->x, player->y, 450);
    if (player->facing_left)
        jo_sprite_disable_horizontal_flip();
}
