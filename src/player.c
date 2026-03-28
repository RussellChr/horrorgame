#include "player.h"
#include "utils.h"
#include "render.h"
#include "world.h"   /* FLOOR_Y */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

Player *player_create(const char *name)
{
    
    Player *p = calloc(1, sizeof(Player));
    if (!p) return NULL;
    strncpy(p->name, name ? name : "Unknown", PLAYER_NAME_MAX - 1);
    p->x = 0.0f;
    p->y = 0.0f;

    /* Initialize custom collider */
    p->collider_w = PLAYER_COLLIDER_W;
    p->collider_h = PLAYER_COLLIDER_H;
    p->collider_offset_x = PLAYER_COLLIDER_OFFSET_X;
    p->collider_offset_y = PLAYER_COLLIDER_OFFSET_Y;

    /* Starting world position (overridden by room spawn on load). */
    p->x = 400.0f;
    p->y = (float)FLOOR_Y;
    p->facing_right = 1;

    animation_init(&p->idle_anim, 2, 1.5f, 1);
    animation_init(&p->walk_anim, 4, 8.0f, 1);
    animation_init(&p->backwards_anim, 2, 8.0f, 1);
    
    p->sprite_texture    = NULL;
    p->sprite_w          = PLAYER_W;
    p->sprite_h          = PLAYER_SPRITE_H;
    p->backwards_texture = NULL;
    return p;
}

void player_destroy(Player *player)
{
    if (!player) return;
    if (player->sprite_texture)
        render_texture_destroy(player->sprite_texture);
    if (player->backwards_texture)
        render_texture_destroy(player->backwards_texture);

    /* Destroy idle textures */
    if (player->idle_texture_north) render_texture_destroy(player->idle_texture_north);
    if (player->idle_texture_south) render_texture_destroy(player->idle_texture_south);
    if (player->idle_texture_east)  render_texture_destroy(player->idle_texture_east);
    if (player->idle_texture_west)  render_texture_destroy(player->idle_texture_west);

    /* Destroy walking textures */
    for (int i = 0; i < 2; i++) {
        if (player->walk_frames_north[i]) render_texture_destroy(player->walk_frames_north[i]);
        if (player->walk_frames_south[i]) render_texture_destroy(player->walk_frames_south[i]);
        if (player->walk_frames_east[i])  render_texture_destroy(player->walk_frames_east[i]);
        if (player->walk_frames_west[i])  render_texture_destroy(player->walk_frames_west[i]);
    }

    free(player);
}

/* ── Sprite ────────────────────────────────────────────────────────────── */

void player_set_sprite(Player *player, SDL_Texture *texture, int w, int h)
{
    if (!player) return;
    if (player->sprite_texture)
        render_texture_destroy(player->sprite_texture);
    player->sprite_texture = texture;
    player->sprite_w       = (w > 0) ? w : PLAYER_W;
    player->sprite_h       = (h > 0) ? h : PLAYER_SPRITE_H;
}

void player_set_backwards_sprite(Player *player, SDL_Texture *texture, int w, int h)
{
    if (!player) return;
    if (player->backwards_texture)
        render_texture_destroy(player->backwards_texture);
    player->backwards_texture = texture;
    player->sprite_w          = (w > 0) ? w : PLAYER_W;
    player->sprite_h          = (h > 0) ? h : PLAYER_SPRITE_H;
}

/* ── Inventory ─────────────────────────────────────────────────────────── */

int player_add_item(Player *player, const Item *item)
{
    if (!player || !item) return 0;
    if (player->inventory_count >= INVENTORY_CAPACITY) return 0;
    player->inventory[player->inventory_count++] = *item;
    return 1;
}

int player_remove_item(Player *player, int item_id)
{
    if (!player) return 0;
    for (int i = 0; i < player->inventory_count; i++) {
        if (player->inventory[i].id == item_id) {
            for (int j = i; j < player->inventory_count - 1; j++)
                player->inventory[j] = player->inventory[j + 1];
            player->inventory_count--;
            return 1;
        }
    }
    return 0;
}

int player_has_item(const Player *player, int item_id)
{
    if (!player) return 0;
    for (int i = 0; i < player->inventory_count; i++)
        if (player->inventory[i].id == item_id) return 1;
    return 0;
}

void player_show_inventory(const Player *player)
{
    if (!player) return;
    printf("\n── Inventory ──────────────────────────────\n");
    if (player->inventory_count == 0) {
        printf("  (empty)\n");
    } else {
        for (int i = 0; i < player->inventory_count; i++) {
            printf("  [%d] %s\n",   i + 1, player->inventory[i].name);
            printf("       %s\n",          player->inventory[i].description);
        }
    }
    printf("───────────────────────────────────────────\n");
}

/* ── Story flags (mask-based API) ─────────────────────────────────────── */

void player_set_flag(Player *player, uint32_t mask)
{
    if (player) player->flags |= mask;
}

void player_clear_flag(Player *player, uint32_t mask)
{
    if (player) player->flags &= ~mask;
}

int player_check_flag(const Player *player, uint32_t mask)
{
    if (!player) return 0;
    return (player->flags & mask) != 0;
}

/* ── Console display ───────────────────────────────────────────────────── */

void player_print_info(const Player *player)
{
    if (!player) return;
    printf("\n── Character: %s ───────────────────────────\n", player->name);
    printf("  Items   : %d / %d\n",
           player->inventory_count, INVENTORY_CAPACITY);
    printf("───────────────────────────────────────────\n");
}

/* ── Visual update ─────────────────────────────────────────────────────── */

void player_update(Player *player, float dt)
{
    if (!player) return;

    /* Determine direction from velocity; on diagonal use the dominant axis */
    float abs_vx = player->vx < 0 ? -player->vx : player->vx;
    float abs_vy = player->vy < 0 ? -player->vy : player->vy;
    if (abs_vx >= abs_vy) {
        if (player->vx < 0)
            player->current_direction = DIRECTION_WEST;
        else if (player->vx > 0)
            player->current_direction = DIRECTION_EAST;
    } else {
        if (player->vy < 0)
            player->current_direction = DIRECTION_NORTH;
        else if (player->vy > 0)
            player->current_direction = DIRECTION_SOUTH;
    }

    /* Animate only when moving */
    if (player->is_moving) {
        player->frame_timer += dt;
        if (player->frame_timer >= player->frame_duration) {
            player->frame_timer = 0.0f;
            player->frame_index = (player->frame_index + 1) % 2;
        }
    } else {
        player->frame_index = 0;
        player->frame_timer = 0.0f;
    }
}

/* ── Visual render ─────────────────────────────────────────────────────── */
/*
 * The player is drawn using the directional texture that matches the current
 * movement state and direction (north/south/east/west).
 *
 *   screen_x / screen_y = top-left corner of the player sprite on screen.
 */
void player_render(Player *player, SDL_Renderer *renderer,
                   int screen_x, int screen_y)
{
    if (!player || !renderer) return;

    SDL_Texture *current_texture = NULL;

    /* Select texture based on movement state and direction */
    if (player->is_moving) {
        /* Walking animation */
        switch (player->current_direction) {
            case DIRECTION_NORTH:
                current_texture = player->walk_frames_north[player->frame_index];
                break;
            case DIRECTION_SOUTH:
                current_texture = player->walk_frames_south[player->frame_index];
                break;
            case DIRECTION_EAST:
                current_texture = player->walk_frames_east[player->frame_index];
                break;
            case DIRECTION_WEST:
                current_texture = player->walk_frames_west[player->frame_index];
                break;
        }
    } else {
        /* Idle pose */
        switch (player->current_direction) {
            case DIRECTION_NORTH:
                current_texture = player->idle_texture_north;
                break;
            case DIRECTION_SOUTH:
                current_texture = player->idle_texture_south;
                break;
            case DIRECTION_EAST:
                current_texture = player->idle_texture_east;
                break;
            case DIRECTION_WEST:
                current_texture = player->idle_texture_west;
                break;
        }
    }

    if (current_texture) {
        SDL_FRect dst = {
            (float)screen_x,
            (float)screen_y,
            (float)PLAYER_W,
            (float)PLAYER_SPRITE_H
        };
        SDL_RenderTexture(renderer, current_texture, NULL, &dst);
    }
}
