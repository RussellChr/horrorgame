#include "player.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

Player *player_create(const char *name) {
    Player *p = calloc(1, sizeof(Player));
    if (!p) return NULL;

    strncpy(p->name, name ? name : "Unknown", PLAYER_NAME_MAX - 1);
    p->health   = 100;
    p->sanity   = 100;
    p->courage  = 50;
    p->current_location_id = 0;
    return p;
}

void player_destroy(Player *player) {
    free(player);
}

/* ── Stat modification ─────────────────────────────────────────────────── */

void player_modify_health(Player *player, int delta) {
    if (!player) return;
    player->health = utils_clamp(player->health + delta, 0, 100);
}

void player_modify_sanity(Player *player, int delta) {
    if (!player) return;
    player->sanity = utils_clamp(player->sanity + delta, 0, 100);
    /* Low sanity erodes courage as well */
    if (delta < 0 && player->sanity < 30) {
        player->courage = utils_clamp(player->courage + delta / 2, 0, 100);
    }
}

void player_modify_courage(Player *player, int delta) {
    if (!player) return;
    player->courage = utils_clamp(player->courage + delta, 0, 100);
}

/* ── Inventory ─────────────────────────────────────────────────────────── */

int player_add_item(Player *player, const Item *item) {
    if (!player || !item) return 0;
    if (player->inventory_count >= INVENTORY_CAPACITY) {
        printf("Your inventory is full.\n");
        return 0;
    }
    player->inventory[player->inventory_count++] = *item;
    return 1;
}

int player_remove_item(Player *player, int item_id) {
    if (!player) return 0;
    for (int i = 0; i < player->inventory_count; i++) {
        if (player->inventory[i].id == item_id) {
            /* shift remaining items left */
            for (int j = i; j < player->inventory_count - 1; j++) {
                player->inventory[j] = player->inventory[j + 1];
            }
            player->inventory_count--;
            return 1;
        }
    }
    return 0;
}

int player_has_item(const Player *player, int item_id) {
    if (!player) return 0;
    for (int i = 0; i < player->inventory_count; i++) {
        if (player->inventory[i].id == item_id) return 1;
    }
    return 0;
}

void player_show_inventory(const Player *player) {
    if (!player) return;
    printf("\n── Inventory ──────────────────────────────\n");
    if (player->inventory_count == 0) {
        printf("  (empty)\n");
    } else {
        for (int i = 0; i < player->inventory_count; i++) {
            printf("  [%d] %s\n", i + 1, player->inventory[i].name);
            printf("       %s\n", player->inventory[i].description);
        }
    }
    printf("───────────────────────────────────────────\n");
}

/* ── Story flags ───────────────────────────────────────────────────────── */

void player_set_flag(Player *player, int bit) {
    if (player) player->flags |= (uint32_t)(1u << bit);
}

void player_clear_flag(Player *player, int bit) {
    if (player) player->flags &= ~(uint32_t)(1u << bit);
}

int player_check_flag(const Player *player, int bit) {
    if (!player) return 0;
    return (player->flags >> bit) & 1;
}

/* ── Display ───────────────────────────────────────────────────────────── */

void player_print_stats(const Player *player) {
    if (!player) return;
    printf("\n── Character: %s ───────────────────────────\n", player->name);
    printf("  Health  : %3d / 100\n", player->health);
    printf("  Sanity  : %3d / 100\n", player->sanity);
    printf("  Courage : %3d / 100\n", player->courage);
    printf("  Items   : %d / %d\n",
           player->inventory_count, INVENTORY_CAPACITY);
    printf("───────────────────────────────────────────\n");
}
