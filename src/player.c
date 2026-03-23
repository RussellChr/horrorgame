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

    /* Advance animation */
    if (player->is_moving) {
        if (player->is_moving_backwards) {
            animation_update(&player->backwards_anim, dt);
        } else {
            animation_update(&player->walk_anim, dt);
        }
    } else {
        animation_update(&player->idle_anim, dt);
    }
}

/* ── Visual render ─────────────────────────────────────────────────────── */
/*
 * The player is drawn using a sprite texture if one is set.
 * When moving backwards the character turns around, achieved by inverting
 * the horizontal flip relative to the normal facing direction.
 *
 *   screen_x / screen_y = top-left corner of the player sprite on screen.
 */
void player_render(Player *player, SDL_Renderer *renderer,
                   int screen_x, int screen_y)
{
    if (!player || !renderer) return;

    int w = 32;  /* sprite frame width  */
    int h = 64;  /* sprite frame height */

    /* When moving backwards the character turns around, so invert the flip. */
    int effective_right = player->is_moving_backwards
                          ? !player->facing_right
                          : player->facing_right;
    SDL_FlipMode flip = effective_right ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;

    SDL_Texture *tex  = NULL;
    int          frame = 0;

    if (player->is_moving_backwards && player->backwards_texture) {
        tex   = player->backwards_texture;
        frame = animation_get_frame(&player->backwards_anim);
    } else if (player->sprite_texture) {
        tex   = player->sprite_texture;
        frame = player->is_moving
                ? animation_get_frame(&player->walk_anim)
                : animation_get_frame(&player->idle_anim);
    }

    if (tex) {
        SDL_FRect src = {(float)(frame * w), 0.0f, (float)w, (float)h};
        SDL_FRect dst = {(float)screen_x, (float)screen_y, (float)w, (float)h};
        SDL_RenderTextureRotated(renderer, tex, &src, &dst, 0, NULL, flip);
    }
}
