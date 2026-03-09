#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>
#include <SDL3/SDL.h>
#include "animation.h"

#define PLAYER_NAME_MAX    64
#define INVENTORY_CAPACITY 16
#define ITEM_NAME_MAX      64
#define ITEM_DESC_MAX      256

/* Sprite dimensions in world pixels. */
#define PLAYER_W  24
#define PLAYER_SPRITE_H  48

/* Movement speed in pixels per second. */
#define PLAYER_SPEED 220.0f

/* ── Item ──────────────────────────────────────────────────────────────── */

typedef struct {
    char name[ITEM_NAME_MAX];
    char description[ITEM_DESC_MAX];
    int  id;
    int  usable;   /* 1 if the item can be used from the inventory */
} Item;

/* ── Player ────────────────────────────────────────────────────────────── */

typedef struct {
    /* Identity & stats */
    char     name[PLAYER_NAME_MAX];
    int      health;        /* 0–100                               */
    int      sanity;        /* 0–100; drops on horror events       */
    int      courage;       /* 0–100; affects dialogue options     */

    /* Inventory */
    int      inventory_count;
    Item     inventory[INVENTORY_CAPACITY];

    /* Story state */
    int      current_location_id;
    int      steps_taken;
    uint32_t flags;         /* bit-field for story flags            */

    /* Visual / movement state */
    float    x, y;          /* world-space position (centre-bottom) */
    float    vx, vy;        /* velocity in pixels/second            */
    int      facing_right;  /* 1 = right, 0 = left                  */
    int      is_moving;     /* 1 if walking this frame              */

    /* Animations */
    Animation idle_anim;
    Animation walk_anim;
} Player;

/* ── API ───────────────────────────────────────────────────────────────── */

Player *player_create(const char *name);
void    player_destroy(Player *player);

/* Stat modification */
void player_modify_health(Player *player, int delta);
void player_modify_sanity(Player *player, int delta);
void player_modify_courage(Player *player, int delta);

/* Inventory */
int  player_add_item(Player *player, const Item *item);
int  player_remove_item(Player *player, int item_id);
int  player_has_item(const Player *player, int item_id);
void player_show_inventory(const Player *player);

/* Status flags – pass a FLAG_* bitmask value (e.g. FLAG_MET_LILY) */
void player_set_flag(Player *player, uint32_t mask);
void player_clear_flag(Player *player, uint32_t mask);
int  player_check_flag(const Player *player, uint32_t mask);

/* Display (console – kept for debugging) */
void player_print_stats(const Player *player);

/* Visual update and render */
void player_update(Player *player, float dt);
void player_render(Player *player, SDL_Renderer *renderer,
                   int screen_x, int screen_y);

#endif /* PLAYER_H */
