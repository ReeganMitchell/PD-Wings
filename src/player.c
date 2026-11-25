#include <jo/jo.h>

#include "game_state.h"
#include "level.h"
#include "player.h"
#include "player_character.h"

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
#define PLAYER_DRAW_DEPTH           (250)

typedef struct
{
    const player_character_definition *definition;
    int animation_ids[PLAYER_ANIMATION_COUNT];
    int first_sprite_id;
    int last_sprite_id;
    bool loaded;
} player_character_runtime;

static player_character_runtime g_characters[2];

static const player_character_definition *current_character(
    const player_state *player)
{
    return g_characters[player->character_index].definition;
}

static player_character_runtime *current_character_runtime(
    const player_state *player)
{
    return &g_characters[player->character_index];
}

static int find_animation_id(const player_character_runtime *character,
                             player_animation_slot slot)
{
    return character->animation_ids[slot];
}

static bool unload_character_animations(player_character_runtime *character)
{
    unsigned int i;

    if (!character->loaded)
        return true;
    if (jo_get_last_sprite_id() != character->last_sprite_id)
    {
        jo_core_error(
            "Cannot unload %s animations: player sprites are not the last "
            "sprites loaded",
            character->definition->name);
        return false;
    }

    for (i = 0; i < PLAYER_ANIMATION_COUNT; ++i)
    {
        if (character->animation_ids[i] >= 0)
        {
            jo_remove_sprite_anim(character->animation_ids[i]);
            character->animation_ids[i] = -1;
        }
    }
    jo_sprite_free_from(character->first_sprite_id);
    character->loaded = false;
    character->first_sprite_id = -1;
    character->last_sprite_id = -1;
    return true;
}

static bool load_character_animations(player_character_runtime *character)
{
    unsigned int i;
    bool success = true;

    if (!jo_fs_cd("CHAR"))
    {
        jo_core_error("Failed to enter character directory: %s",
                      jo_get_last_error());
        return false;
    }
    if (!jo_fs_cd(character->definition->directory))
    {
        jo_core_error("Failed to enter %s character directory: %s",
                      character->definition->name, jo_get_last_error());
        if (!jo_fs_cd(JO_PARENT_DIR))
            jo_core_error("Failed to return to CD root: %s",
                          jo_get_last_error());
        return false;
    }

    for (i = 0; i < PLAYER_ANIMATION_COUNT; ++i)
        character->animation_ids[i] = -1;
    character->first_sprite_id = jo_get_last_sprite_id() + 1;
    character->last_sprite_id = -1;
    character->loaded = false;

    for (i = 0; i < character->definition->animation_count; ++i)
    {
        const player_animation_definition *animation =
            &character->definition->animations[i];
        jo_tile tiles[PLAYER_MAX_ANIMATION_FRAMES];
        unsigned int frame;
        int sprite_id;

        if (animation->frame_count > PLAYER_MAX_ANIMATION_FRAMES)
        {
            jo_core_error("%s animation %s has too many frames",
                          character->definition->name,
                          animation->filename);
            success = false;
            break;
        }
        for (frame = 0; frame < animation->frame_count; ++frame)
        {
            tiles[frame].x = frame * animation->frame_width;
            tiles[frame].y = 0;
            tiles[frame].width = animation->frame_width;
            tiles[frame].height = animation->frame_height;
        }
        sprite_id = jo_sprite_add_bin_tileset(
            JO_CURRENT_DIR, animation->filename, JO_COLOR_Green, tiles,
            animation->frame_count);
        if (sprite_id < 0)
        {
            jo_core_error("Failed to load %s animation %s: %s",
                          character->definition->name,
                          animation->filename, jo_get_last_error());
            success = false;
            break;
        }
        character->animation_ids[animation->slot] = jo_create_sprite_anim(
            (unsigned short)sprite_id, animation->frame_count,
            animation->frame_rate);
        if (character->animation_ids[animation->slot] < 0)
        {
            jo_core_error("Failed to create %s animation %s",
                          character->definition->name,
                          animation->filename);
            success = false;
            break;
        }
    }
    if (success)
    {
        character->last_sprite_id = jo_get_last_sprite_id();
        character->loaded = true;
    }

    if (!jo_fs_cd(JO_PARENT_DIR))
    {
        jo_core_error("Failed to return from %s character directory: %s",
                      character->definition->name, jo_get_last_error());
        success = false;
    }
    else if (!jo_fs_cd(JO_PARENT_DIR))
    {
        jo_core_error("Failed to return to CD root: %s", jo_get_last_error());
        success = false;
    }
    if (!success && character->first_sprite_id <= jo_get_last_sprite_id())
    {
        character->last_sprite_id = jo_get_last_sprite_id();
        character->loaded = true;
        if (!unload_character_animations(character))
            return false;
    }
    return success;
}

static const player_animation_definition *find_animation_definition(
    const player_character_definition *character,
    player_animation_slot slot)
{
    unsigned int i;

    for (i = 0; i < character->animation_count; ++i)
    {
        if (character->animations[i].slot == slot)
            return &character->animations[i];
    }
    return JO_NULL;
}

static int find_step_up_correction(const game_state *game)
{
    const player_state *player = &game->player;
    int foot_y = game->level.map_y + player->y +
                 PLAYER_COLLISION_BOTTOM;
    int movement_delta = JO_FIXED_TO_INT(
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
    int movement_delta = JO_FIXED_TO_INT(
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
    if (player->physics.speed_y < JO_FIXED_0)
    {
        player->physics.is_in_air = true;
        return false;
    }
    if (was_grounded && player->physics.speed != JO_FIXED_0)
    {
        step_up = find_step_up_correction(game);
        if (step_up < 0)
        {
            player->y += step_up;
            player->can_jump = true;
            player->physics.is_in_air = false;
            player->vertical_remainder = JO_FIXED_0;
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
    if (distance < 0 && player->physics.speed_y == JO_FIXED_0)
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
    unsigned int i;

    g_characters[0].definition = player_rei_definition();
    g_characters[1].definition = player_sam_definition();
    for (i = 0; i < sizeof(g_characters) / sizeof(g_characters[0]); ++i)
    {
        unsigned int animation;

        g_characters[i].first_sprite_id = -1;
        g_characters[i].last_sprite_id = -1;
        g_characters[i].loaded = false;
        for (animation = 0; animation < PLAYER_ANIMATION_COUNT; ++animation)
            g_characters[i].animation_ids[animation] = -1;
    }
    player->character_index = 0;
    if (!load_character_animations(&g_characters[player->character_index]))
        return false;
    player->active_animation_id = -1;
    player->down_move_active = false;
    player->attack_animation_active = false;
    return true;
}

void player_reset(player_state *player)
{
    player_character_runtime *character =
        current_character_runtime(player);
    unsigned int animation;

    player->x = PLAYER_START_X;
    player->y = PLAYER_START_Y;
    player->facing_left = false;
    player->can_jump = true;
    player->wall_collision = false;
    player->horizontal_remainder = JO_FIXED_0;
    player->vertical_remainder = JO_FIXED_0;
    player->animation_frame_skip = 0;
    for (animation = 0; animation < PLAYER_ANIMATION_COUNT; ++animation)
    {
        if (character->animation_ids[animation] >= 0)
            jo_reset_sprite_anim(character->animation_ids[animation]);
    }
    player->down_move_active = false;
    player->attack_animation_active = false;
    player->active_animation_id = find_animation_id(
        current_character_runtime(player), PLAYER_ANIMATION_IDLE);
    jo_physics_init_for_sonic(&player->physics);
    player->physics.max_speed =
        JO_INT_TO_FIXED(current_character(player)->max_speed);
    player->physics.acceleration_strength =
        current_character(player)->ground_acceleration;
    player->physics.air_acceleration_strength =
        current_character(player)->air_acceleration;
    player->physics.friction = current_character(player)->friction;
}

bool player_select_next_character(player_state *player)
{
    const player_character_definition *previous_character =
        current_character(player);
    int previous_character_index = player->character_index;
    unsigned int character_count =
        sizeof(g_characters) / sizeof(g_characters[0]);

    if (!unload_character_animations(
            current_character_runtime(player)))
        return false;
    player->active_animation_id = -1;
    player->character_index = (player->character_index + 1) %
        character_count;
    if (!load_character_animations(current_character_runtime(player)))
    {
        player->character_index = previous_character_index;
        if (!load_character_animations(current_character_runtime(player)))
            jo_core_error("Failed to restore %s character animations",
                          previous_character->name);
        return false;
    }
    player->attack_animation_active = false;
    player->active_animation_id = -1;
    player->y += previous_character->sprite_height -
        current_character(player)->sprite_height;
    player->physics.max_speed =
        JO_INT_TO_FIXED(current_character(player)->max_speed);
    player->physics.acceleration_strength =
        current_character(player)->ground_acceleration;
    player->physics.air_acceleration_strength =
        current_character(player)->air_acceleration;
    player->physics.friction = current_character(player)->friction;
    if (player->physics.speed > player->physics.max_speed)
        player->physics.speed = player->physics.max_speed;
    else if (player->physics.speed < -player->physics.max_speed)
        player->physics.speed = -player->physics.max_speed;
    return true;
}

void player_set_down_move(player_state *player, bool active)
{
    player->down_move_active = active;
}

void player_start_attack(player_state *player)
{
    player->attack_animation_active = true;
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
    int vertical_delta;
    int direction;
    int step;

    if (check_vertical_collision(game))
    {
        player->physics.speed_y = JO_FIXED_0;
        player->vertical_remainder = JO_FIXED_0;
    }
    else
    {
        jo_physics_apply_gravity(&player->physics);
        player->vertical_remainder += player->physics.speed_y;
        vertical_delta = JO_FIXED_TO_INT(player->vertical_remainder);
        player->vertical_remainder -= JO_INT_TO_FIXED(vertical_delta);
        player->y += vertical_delta;
    }

    player->horizontal_remainder += player->physics.speed;
    horizontal_delta = JO_FIXED_TO_INT(player->horizontal_remainder);
    player->horizontal_remainder -= JO_INT_TO_FIXED(horizontal_delta);
    direction = horizontal_delta > 0 ? 1 :
        horizontal_delta < 0 ? -1 : 0;
    player->wall_collision = false;
    for (step = 0; step < JO_ABS(horizontal_delta); ++step)
    {
        int next_map_x = game->level.map_x + direction;

        if (has_wall_at_map_x(game, next_map_x, direction))
        {
            player->wall_collision = true;
            player->physics.speed = JO_FIXED_0;
            player->horizontal_remainder = JO_FIXED_0;
            break;
        }
        game->level.map_x = next_map_x;
    }

    if (!player->wall_collision)
    {
        direction = player->physics.speed > JO_FIXED_0 ? 1 :
            player->physics.speed < JO_FIXED_0 ? -1 :
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
    const player_character_definition *character = current_character(player);
    player_character_runtime *runtime = current_character_runtime(player);
    const player_animation_definition *animation;
    player_animation_slot slot;
    bool is_attack_animation = false;
    int animation_id;
    unsigned char frame_rate;
    unsigned short frame;

    if (player->attack_animation_active)
    {
        if (!player->physics.is_in_air &&
            (player->character_index != 0 ||
             player->physics.speed == JO_FIXED_0))
            slot = character->grounded_attack;
        else
            slot = character->aerial_attack;
        is_attack_animation = true;
    }
    else if (player->down_move_active && !player->physics.is_in_air &&
             player->physics.speed != JO_FIXED_0)
        slot = character->down_move_animation;
    else if (player->wall_collision && player->physics.is_in_air)
        slot = PLAYER_ANIMATION_WALL;
    else if (player->physics.is_in_air)
        slot = PLAYER_ANIMATION_JUMP;
    else if (jo_physics_is_standing(&player->physics))
        slot = PLAYER_ANIMATION_IDLE;
    else
        slot = PLAYER_ANIMATION_RUN;

    animation = find_animation_definition(character, slot);
    if (animation == JO_NULL)
    {
        slot = jo_physics_is_standing(&player->physics) ?
            PLAYER_ANIMATION_IDLE : PLAYER_ANIMATION_RUN;
        animation = find_animation_definition(character, slot);
        is_attack_animation = false;
        player->attack_animation_active = false;
    }
    if (animation == JO_NULL)
        return;

    animation_id = find_animation_id(runtime, slot);

    if (animation_id != player->active_animation_id)
    {
        if (player->active_animation_id >= 0)
            jo_reset_sprite_anim(player->active_animation_id);
        jo_reset_sprite_anim(animation_id);
        player->active_animation_id = animation_id;
        player->animation_frame_skip = 0;
    }
    frame_rate = animation->frame_rate;
    if (slot == PLAYER_ANIMATION_RUN)
    {
        int speed_step = JO_FIXED_TO_INT(JO_ABS(player->physics.speed));

        frame_rate = (unsigned char)(9 - JO_MIN(speed_step, 6));
    }
    jo_set_sprite_anim_frame_rate(animation_id, frame_rate);
    if (player->animation_frame_skip >= frame_rate)
    {
        player->animation_frame_skip = 0;
        frame = (unsigned short)(jo_get_sprite_anim_frame(animation_id) + 1);
        if (frame >= jo_get_sprite_anim_frame_count(animation_id))
        {
            if (animation->loops)
                frame = 0;
            else
            {
                frame = (unsigned short)(
                    jo_get_sprite_anim_frame_count(animation_id) - 1);
                if (is_attack_animation)
                    player->attack_animation_active = false;
            }
        }
        jo_set_sprite_anim_frame(animation_id, frame);
    }
    else
        ++player->animation_frame_skip;
}

void player_draw(const player_state *player)
{
    int sprite_id;

    if (player->active_animation_id < 0)
        return;
    sprite_id = jo_get_anim_sprite(player->active_animation_id);

    if (player->facing_left)
        jo_sprite_enable_horizontal_flip();
    jo_sprite_draw3D2(sprite_id, player->x, player->y, PLAYER_DRAW_DEPTH);
    if (player->facing_left)
        jo_sprite_disable_horizontal_flip();
}
