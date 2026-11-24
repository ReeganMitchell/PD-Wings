#include <jo/jo.h>

#include "game_state.h"
#include "level.h"
#include "player.h"

#define PLAYER_START_X              (100)
#define PLAYER_START_Y              (250)
#define PLAYER_WIDTH_2              (GAME_PLAYER_WIDTH / 2)
#define PLAYER_COLLISION_LEFT       (14)
#define PLAYER_COLLISION_RIGHT      (35)
#define PLAYER_COLLISION_TOP        (10)
#define PLAYER_COLLISION_BOTTOM     (40)
#define PLAYER_WALL_PROBE_BOTTOM    (PLAYER_COLLISION_BOTTOM - 16)
#define PLAYER_STEP_UP_MAX          (8)
#define PLAYER_STEP_DOWN_MAX        (8)
#define JUMP_PER_PIXEL_TOLERANCE    (20)
#define PLAYER_MAX_ANIMATION_FRAMES (15)
#define PLAYER_IDLE_FRAME_RATE      (6)
#define PLAYER_DRAW_DEPTH           (250)

typedef struct
{
    const char *name;
    const char *directory;
    const char *idle_filename;
    const char *run_filename;
    unsigned short idle_frame_count;
    unsigned short run_frame_count;
    int max_speed;
    float ground_acceleration;
    float air_acceleration;
    float friction;
    int sprite_height;
    bool shares_animation_sheet;
    int idle_anim_id;
    int run_anim_id;
} player_character;

static player_character g_characters[] =
{
    {"REI", "REI", "IDLE.BIN", "RUN.BIN", 15, 8, 3, 0.25f, 0.09375f, 0.08f, 48, false, -1, -1},
    {"SAM", "SAM", "IDLE.BIN", "RUN.BIN", 10, 8, 4, 0.25f, 0.09375f, 0.08f, 48, false, -1, -1}
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

static int find_step_up_correction(const game_state *game)
{
    const player_state *player = &game->player;
    int foot_y = game->level.map_y + player->y +
                 PLAYER_COLLISION_BOTTOM;
    int movement_delta = (int)(
        player->horizontal_remainder + player->physics.speed);
    int probe_x;
    int probe_y;

    if (movement_delta == 0)
        return 0;
    probe_x = game->level.map_x + player->x + PLAYER_WIDTH_2 +
              movement_delta;

    for (probe_y = foot_y - 1;
         probe_y >= foot_y - PLAYER_STEP_UP_MAX;
         --probe_y)
    {
        int distance = jo_map_per_pixel_vertical_collision(
            GAME_COLLISION_MAP_ID, probe_x, probe_y, JO_NULL);
        int correction;

        if (distance == JO_MAP_NO_COLLISION)
            continue;
        correction = probe_y + distance - foot_y;
        if (correction < 0 &&
            correction >= -PLAYER_STEP_UP_MAX)
            return correction;
    }

    return 0;
}

static int find_step_down_correction(const game_state *game)
{
    const player_state *player = &game->player;
    int foot_y = game->level.map_y + player->y +
                 PLAYER_COLLISION_BOTTOM;
    int movement_delta = (int)(
        player->horizontal_remainder + player->physics.speed);
    int probe_x;
    int probe_y;

    if (movement_delta == 0)
        return 0;
    probe_x = game->level.map_x + player->x + PLAYER_WIDTH_2 +
              movement_delta;

    for (probe_y = foot_y;
         probe_y <= foot_y + PLAYER_STEP_DOWN_MAX;
         ++probe_y)
    {
        int distance = jo_map_per_pixel_vertical_collision(
            GAME_COLLISION_MAP_ID, probe_x, probe_y, JO_NULL);
        int correction;

        if (distance == JO_MAP_NO_COLLISION)
            continue;
        correction = probe_y + distance - foot_y;
        if (correction > 0 &&
            correction <= PLAYER_STEP_DOWN_MAX)
            return correction;
    }

    return 0;
}

static bool check_vertical_collision(game_state *game)
{
    player_state *player = &game->player;
    int distance;
    int step_up;
    int step_down;
    bool was_grounded = !player->physics.is_in_air;

    player->can_jump = false;
    if (player->physics.speed_y < 0.0f)
    {
        player->physics.is_in_air = true;
        return false;
    }
    if (was_grounded && player->physics.speed != 0.0f)
    {
        step_up = find_step_up_correction(game);
        if (step_up < 0)
        {
            player->y += step_up;
            player->can_jump = true;
            player->physics.is_in_air = false;
            return true;
        }
        step_down = find_step_down_correction(game);
        if (step_down > 0)
        {
            player->y += step_down;
            player->can_jump = true;
            player->physics.is_in_air = false;
            return true;
        }
    }
    distance = jo_map_per_pixel_vertical_collision(
        GAME_COLLISION_MAP_ID,
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

static bool has_wall_at_map_x(const game_state *game,
                              int map_x,
                              int direction)
{
    const player_state *player = &game->player;
    int wall_x = map_x + player->x +
        (direction > 0 ? PLAYER_COLLISION_RIGHT - 1 :
         PLAYER_COLLISION_LEFT);
    int y;

    for (y = game->level.map_y + player->y + PLAYER_COLLISION_TOP;
         y < game->level.map_y + player->y +
                 PLAYER_WALL_PROBE_BOTTOM;
         ++y)
    {
        int distance = jo_map_per_pixel_vertical_collision(
            GAME_COLLISION_MAP_ID, wall_x, y, JO_NULL);

        if (distance != JO_MAP_NO_COLLISION && distance <= 0)
            return true;
    }
    return false;
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
    player->animation_frame_skip = 0;
    player->active_animation_id = -1;
    for (i = 0; i < (int)(sizeof(g_characters) / sizeof(g_characters[0])); ++i)
    {
        jo_reset_sprite_anim(g_characters[i].idle_anim_id);
        jo_reset_sprite_anim(g_characters[i].run_anim_id);
    }
    jo_physics_init_for_sonic(&player->physics);
    player->physics.max_speed = current_character(player)->max_speed;
    player->physics.acceleration_strength =
        current_character(player)->ground_acceleration;
    player->physics.air_acceleration_strength =
        current_character(player)->air_acceleration;
    player->physics.friction = current_character(player)->friction;
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
    player->physics.acceleration_strength =
        current_character(player)->ground_acceleration;
    player->physics.air_acceleration_strength =
        current_character(player)->air_acceleration;
    player->physics.friction = current_character(player)->friction;
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
    int direction;
    int step;

    if (check_vertical_collision(game))
        player->physics.speed_y = 0.0f;
    else
    {
        jo_physics_apply_gravity(&player->physics);
        player->y += (int)player->physics.speed_y;
    }

    player->horizontal_remainder += player->physics.speed;
    horizontal_delta = (int)player->horizontal_remainder;
    player->horizontal_remainder -= horizontal_delta;
    direction = horizontal_delta > 0 ? 1 :
        horizontal_delta < 0 ? -1 : 0;
    player->wall_collision = false;
    for (step = 0; step < JO_ABS(horizontal_delta); ++step)
    {
        int next_map_x = game->level.map_x + direction;

        if (has_wall_at_map_x(game, next_map_x, direction))
        {
            player->wall_collision = true;
            player->physics.speed = 0.0f;
            player->horizontal_remainder = 0.0f;
            break;
        }
        game->level.map_x = next_map_x;
    }

    if (!player->wall_collision)
    {
        direction = player->physics.speed > 0.0f ? 1 :
            player->physics.speed < 0.0f ? -1 :
            player->facing_left ? -1 : 1;
        if (has_wall_at_map_x(
                game, game->level.map_x + direction, direction))
            player->wall_collision = true;
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
    unsigned char frame_rate;
    unsigned short frame;

    if (animation_id != player->active_animation_id)
    {
        if (player->active_animation_id >= 0)
            jo_reset_sprite_anim(player->active_animation_id);
        jo_reset_sprite_anim(animation_id);
        player->active_animation_id = animation_id;
        player->animation_frame_skip = 0;
    }
    if (animation_id == character->idle_anim_id)
        frame_rate = jo_get_sprite_anim_frame_rate(animation_id);
    else
    {
        speed_step = (int)JO_ABS(player->physics.speed);
        frame_rate = (unsigned char)(9 - JO_MIN(speed_step, 6));
        jo_set_sprite_anim_frame_rate(animation_id, frame_rate);
    }
    if (player->animation_frame_skip >= frame_rate)
    {
        player->animation_frame_skip = 0;
        frame = (unsigned short)(jo_get_sprite_anim_frame(animation_id) + 1);
        if (frame >= jo_get_sprite_anim_frame_count(animation_id))
            frame = 0;
        jo_set_sprite_anim_frame(animation_id, frame);
    }
    else
        ++player->animation_frame_skip;
}

void player_draw(const player_state *player)
{
    int sprite_id = jo_get_anim_sprite(player->active_animation_id);

    if (player->facing_left)
        jo_sprite_enable_horizontal_flip();
    jo_sprite_draw3D2(sprite_id, player->x, player->y, PLAYER_DRAW_DEPTH);
    if (player->facing_left)
        jo_sprite_disable_horizontal_flip();
}
