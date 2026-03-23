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

/* Direction constants for current_direction field. */
#define DIRECTION_NORTH 0
#define DIRECTION_SOUTH 1
#define DIRECTION_EAST  2
#define DIRECTION_WEST  3

/* ── Item ──────────────────────────────────────────────────────────────── */

typedef struct {
    char name[ITEM_NAME_MAX];
    char description[ITEM_DESC_MAX];
    int  id;
    int  usable;   /* 1 if the item can be used from the inventory */
} Item;

/* ── Player ────────────────────────────────────────────────────────────── */

typedef struct {
    /* Identity */
    char     name[PLAYER_NAME_MAX];

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
    int      is_moving_backwards; /* 1 if moving backward this frame */

    /* Animations */
    Animation idle_anim;
    Animation walk_anim;
    Animation backwards_anim;  /* backward movement animation        */

    /* Sprites */
    SDL_Texture *sprite_texture;    /* forward/idle sprite sheet    */
    int          sprite_w;
    int          sprite_h;
    SDL_Texture *backwards_texture; /* backward movement sprite sheet */

    /* Idle textures for each direction */
    SDL_Texture *idle_texture_north;
    SDL_Texture *idle_texture_south;
    SDL_Texture *idle_texture_east;
    SDL_Texture *idle_texture_west;

    /* Walking textures for each direction (2 frames) */
    SDL_Texture *walk_frames_north[2];
    SDL_Texture *walk_frames_south[2];
    SDL_Texture *walk_frames_east[2];
    SDL_Texture *walk_frames_west[2];

    /* Animation state */
    int   current_direction; /* NORTH=0, SOUTH=1, EAST=2, WEST=3 */
    int   frame_index;
    float frame_timer;
    float frame_duration;
} Player;

/* ── API ───────────────────────────────────────────────────────────────── */

Player *player_create(const char *name);
void    player_destroy(Player *player);

/* Sprite */
void player_set_sprite(Player *player, SDL_Texture *texture, int w, int h);
void player_set_backwards_sprite(Player *player, SDL_Texture *texture, int w, int h);

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
void player_print_info(const Player *player);

/* Visual update and render */
void player_update(Player *player, float dt);
void player_render(Player *player, SDL_Renderer *renderer,
                   int screen_x, int screen_y);

#endif /* PLAYER_H */
