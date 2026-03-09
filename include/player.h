#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

#define PLAYER_NAME_MAX    64
#define INVENTORY_CAPACITY 16
#define ITEM_NAME_MAX      64
#define ITEM_DESC_MAX      256

/* ── Item ──────────────────────────────────────────────────────────────── */

typedef struct {
    char     name[ITEM_NAME_MAX];
    char     description[ITEM_DESC_MAX];
    int      id;
    int      usable;   /* 1 if the item can be used from the inventory    */
} Item;

/* ── Player ────────────────────────────────────────────────────────────── */

typedef struct {
    char     name[PLAYER_NAME_MAX];
    int      health;        /* 0–100                                       */
    int      sanity;        /* 0–100; drops on horror events               */
    int      courage;       /* 0–100; affects dialogue options             */
    int      inventory_count;
    Item     inventory[INVENTORY_CAPACITY];
    int      current_location_id;
    int      steps_taken;
    uint32_t flags;         /* bit-field for story flags                   */
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

/* Status flags (bit operations on player->flags) */
void player_set_flag(Player *player, int bit);
void player_clear_flag(Player *player, int bit);
int  player_check_flag(const Player *player, int bit);

/* Display */
void player_print_stats(const Player *player);

#endif /* PLAYER_H */
