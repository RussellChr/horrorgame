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
    p->health   = 100;
    p->sanity   = 100;
    p->courage  = 50;

    /* Starting world position (overridden by room spawn on load). */
    p->x = 400.0f;
    p->y = (float)FLOOR_Y;
    p->facing_right = 1;

    animation_init(&p->idle_anim, 2, 1.5f, 1);
    animation_init(&p->walk_anim, 4, 8.0f, 1);

    return p;
}

void player_destroy(Player *player)
{
    free(player);
}

/* ── Stat modification ─────────────────────────────────────────────────── */

void player_modify_health(Player *player, int delta)
{
    if (!player) return;
    player->health = utils_clamp(player->health + delta, 0, 100);
}

void player_modify_sanity(Player *player, int delta)
{
    if (!player) return;
    player->sanity = utils_clamp(player->sanity + delta, 0, 100);
    if (delta < 0 && player->sanity < 30)
        player->courage = utils_clamp(player->courage + delta / 2, 0, 100);
}

void player_modify_courage(Player *player, int delta)
{
    if (!player) return;
    player->courage = utils_clamp(player->courage + delta, 0, 100);
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

void player_print_stats(const Player *player)
{
    if (!player) return;
    printf("\n── Character: %s ───────────────────────────\n", player->name);
    printf("  Health  : %3d / 100\n", player->health);
    printf("  Sanity  : %3d / 100\n", player->sanity);
    printf("  Courage : %3d / 100\n", player->courage);
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
        animation_update(&player->walk_anim, dt);
    } else {
        animation_update(&player->idle_anim, dt);
    }
}

/* ── Visual render ─────────────────────────────────────────────────────── */
/*
 * The player is drawn as a simple pixel-art silhouette using coloured
 * rectangles (no external image required).
 *
 *   screen_x / screen_y = top-left corner of the player sprite on screen.
 */
void player_render(Player *player, SDL_Renderer *renderer,
                   int screen_x, int screen_y)
{
    if (!player || !renderer) return;

    int w = PLAYER_W;
    int h = PLAYER_SPRITE_H;

    /* Idle bobbing: shift up by 0–2 px depending on idle frame. */
    int bob = 0;
    if (!player->is_moving) {
        int f = animation_get_frame(&player->idle_anim);
        bob   = (f == 0) ? 0 : -2;
    }
    int sy = screen_y + bob;

    /* Walk stride: shift foot rect left/right by frame. */
    int stride = 0;
    if (player->is_moving) {
        int f = animation_get_frame(&player->walk_anim);
        static const int stride_offsets[4] = {0, 2, 0, -2};
        stride = stride_offsets[f];
        if (!player->facing_right) stride = -stride;
    }

    /* ── Draw body parts ── */
    /* Torso */
    render_filled_rect(renderer,
        screen_x + 4, sy + h/3,
        w - 8, h * 2/5,
        80, 80, 110, 255);

    /* Head */
    render_filled_rect(renderer,
        screen_x + 5, sy + 2,
        w - 10, h/4,
        200, 170, 140, 255);

    /* Hair */
    render_filled_rect(renderer,
        screen_x + 4, sy,
        w - 8, h/8,
        60, 40, 20, 255);

    /* Legs */
    int leg_y = sy + h * 3/4;
    render_filled_rect(renderer,
        screen_x + 3 + stride, leg_y,
        (w - 6) / 2 - 1, h/4,
        60, 60, 90, 255);
    render_filled_rect(renderer,
        screen_x + w/2 + 1 - stride, leg_y,
        (w - 6) / 2 - 1, h/4,
        60, 60, 90, 255);

    /* Eyes (direction-dependent) */
    int eye_x = player->facing_right
                ? (screen_x + w - 8)
                : (screen_x + 4);
    render_filled_rect(renderer,
        eye_x, sy + h/4 - 2,
        3, 3,
        30, 20, 20, 255);

    /* Sanity effect: darken the character if sanity is low */
    if (player->sanity < 30) {
        /* Draw a semi-transparent dark overlay */
        SDL_SetRenderDrawColor(renderer, 0, 0, 40,
            (Uint8)(180 - player->sanity * 3));
        SDL_FRect overlay = {
            (float)screen_x, (float)sy, (float)w, (float)h
        };
        SDL_RenderFillRect(renderer, &overlay);
    }
}
