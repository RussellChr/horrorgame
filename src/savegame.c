#include "savegame.h"
/* savegame.h includes game.h which transitively provides player.h,
 * story.h, camera.h, world.h, and all other needed headers. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── Location name helper ────────────────────────────────────────────────── */

static const char *loc_name(int loc_id)
{
    switch (loc_id) {
    case 0: return "Archive";
    case 1: return "Lab";
    case 2: return "Hallway";
    default: return "Room";
    }
}

/* ── savegame_slot_path ──────────────────────────────────────────────────── */

void savegame_slot_path(int slot, char *buf, int buf_size)
{
    if (!buf || buf_size <= 0) return;
    snprintf(buf, (size_t)buf_size, "save_slot_%d.sav", slot);
}

/* ── savegame_exists_slot ────────────────────────────────────────────────── */

int savegame_exists_slot(int slot)
{
    char path[64];
    savegame_slot_path(slot, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* ── savegame_get_slot_info ──────────────────────────────────────────────── */

void savegame_get_slot_info(int slot, char *buf, int buf_size)
{
    if (!buf || buf_size <= 0) return;

    char path[64];
    savegame_slot_path(slot, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(buf, (size_t)buf_size, "Empty");
        return;
    }

    int loc_id = -1;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        int v;
        if (sscanf(line, "PLAYER_LOC %d", &v) == 1) {
            loc_id = v;
            break;
        }
    }
    fclose(f);

    if (loc_id >= 0)
        snprintf(buf, (size_t)buf_size, "%s", loc_name(loc_id));
    else
        snprintf(buf, (size_t)buf_size, "Saved");
}

/* ── savegame_save_slot ──────────────────────────────────────────────────── */

int savegame_save_slot(const Game *game, int slot)
{
    if (!game || !game->player) return 0;

    char path[64];
    savegame_slot_path(slot, path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return 0;

    const Player *p = game->player;

    fprintf(f, "VERSION 2\n");

    /* Player position and location */
    fprintf(f, "PLAYER_X %f\n",   p->x);
    fprintf(f, "PLAYER_Y %f\n",   p->y);
    fprintf(f, "PLAYER_LOC %d\n", p->current_location_id);

    /* Player story flags */
    fprintf(f, "PLAYER_FLAGS %u\n", p->flags);

    /* Player facing / direction */
    fprintf(f, "PLAYER_FACING %d\n", p->facing_right);
    fprintf(f, "PLAYER_DIR %d\n",    p->current_direction);

    /* Inventory */
    fprintf(f, "INV_COUNT %d\n", p->inventory_count);
    for (int i = 0; i < p->inventory_count; i++) {
        const Item *it = &p->inventory[i];
        fprintf(f, "ITEM_ID %d\n",     it->id);
        fprintf(f, "ITEM_USABLE %d\n", it->usable);
        fprintf(f, "ITEM_NAME %s\n",   it->name);
        fprintf(f, "ITEM_DESC %s\n",   it->description);
    }

    /* Enemy */
    fprintf(f, "ENEMY_ACTIVE %d\n", game->enemy.active);
    fprintf(f, "ENEMY_STATE %d\n",  game->enemy.state);
    fprintf(f, "ENEMY_X %f\n",      game->enemy.x);
    fprintf(f, "ENEMY_Y %f\n",      game->enemy.y);
    fprintf(f, "ENEMY_WP %d\n",     game->enemy.current_waypoint);
    fprintf(f, "ENEMY_PDIR %d\n",   game->enemy.patrol_dir);

    /* Story */
    if (game->story) {
        fprintf(f, "STORY_CHAPTER %d\n", game->story->current_chapter);
        fprintf(f, "STORY_CHOICES %d\n", game->story->choices_made);
    } else {
        fprintf(f, "STORY_CHAPTER 0\n");
        fprintf(f, "STORY_CHOICES 0\n");
    }

    /* Misc game flags */
    fprintf(f, "CUTSCENE_PLAYED %d\n", game->security_cutscene_played);
    fprintf(f, "FLASHLIGHT %d\n",      game->flashlight_active);
    fprintf(f, "GASMASK %d\n",         game->gasmask_active);
    fprintf(f, "LAB_GAS %f\n",         game->lab_gas_timer);

    fclose(f);
    return 1;
}

/* ── savegame_load_slot ──────────────────────────────────────────────────── */

int savegame_load_slot(Game *game, int slot)
{
    if (!game || !game->player) return 0;

    char path[64];
    savegame_slot_path(slot, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    Player *p = game->player;
    char    line[512];
    int     version  = 0;

    /* Temporary inventory storage */
    int  inv_count = 0;
    Item inv_items[INVENTORY_CAPACITY];
    memset(inv_items, 0, sizeof(inv_items));
    int  cur_item = -1;

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
        if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';

        char key[64];
        char val[448];
        if (sscanf(line, "%63s %447[^\n]", key, val) < 2) continue;

        if      (strcmp(key, "VERSION")       == 0) version = atoi(val);
        else if (strcmp(key, "PLAYER_X")      == 0) p->x = (float)atof(val);
        else if (strcmp(key, "PLAYER_Y")      == 0) p->y = (float)atof(val);
        else if (strcmp(key, "PLAYER_LOC")    == 0) p->current_location_id = atoi(val);
        else if (strcmp(key, "PLAYER_FLAGS")  == 0) p->flags = (uint32_t)strtoul(val, NULL, 10);
        else if (strcmp(key, "PLAYER_FACING") == 0) p->facing_right = atoi(val);
        else if (strcmp(key, "PLAYER_DIR")    == 0) p->current_direction = atoi(val);
        else if (strcmp(key, "INV_COUNT") == 0) {
            inv_count = atoi(val);
            if (inv_count > INVENTORY_CAPACITY) inv_count = INVENTORY_CAPACITY;
            cur_item  = -1;
        }
        else if (strcmp(key, "ITEM_ID")     == 0) {
            cur_item++;
            if (cur_item < inv_count) inv_items[cur_item].id = atoi(val);
        }
        else if (strcmp(key, "ITEM_USABLE") == 0) {
            if (cur_item >= 0 && cur_item < inv_count)
                inv_items[cur_item].usable = atoi(val);
        }
        else if (strcmp(key, "ITEM_NAME") == 0) {
            if (cur_item >= 0 && cur_item < inv_count) {
                strncpy(inv_items[cur_item].name, val, ITEM_NAME_MAX - 1);
                inv_items[cur_item].name[ITEM_NAME_MAX - 1] = '\0';
            }
        }
        else if (strcmp(key, "ITEM_DESC") == 0) {
            if (cur_item >= 0 && cur_item < inv_count) {
                strncpy(inv_items[cur_item].description, val, ITEM_DESC_MAX - 1);
                inv_items[cur_item].description[ITEM_DESC_MAX - 1] = '\0';
            }
        }
        else if (strcmp(key, "ENEMY_ACTIVE") == 0) game->enemy.active           = atoi(val);
        else if (strcmp(key, "ENEMY_STATE")  == 0) game->enemy.state            = atoi(val);
        else if (strcmp(key, "ENEMY_X")      == 0) game->enemy.x                = (float)atof(val);
        else if (strcmp(key, "ENEMY_Y")      == 0) game->enemy.y                = (float)atof(val);
        else if (strcmp(key, "ENEMY_WP")     == 0) game->enemy.current_waypoint = atoi(val);
        else if (strcmp(key, "ENEMY_PDIR")   == 0) game->enemy.patrol_dir       = atoi(val);
        else if (strcmp(key, "STORY_CHAPTER")   == 0) {
            if (game->story) game->story->current_chapter = atoi(val);
        }
        else if (strcmp(key, "STORY_CHOICES")   == 0) {
            if (game->story) game->story->choices_made    = atoi(val);
        }
        else if (strcmp(key, "CUTSCENE_PLAYED") == 0) game->security_cutscene_played = atoi(val);
        else if (strcmp(key, "FLASHLIGHT")      == 0) game->flashlight_active        = atoi(val);
        else if (strcmp(key, "GASMASK")         == 0) game->gasmask_active           = atoi(val);
        else if (strcmp(key, "LAB_GAS")         == 0) game->lab_gas_timer            = (float)atof(val);
    }

    fclose(f);

    if (version < 1) return 0;

    /* Restore inventory */
    p->inventory_count = 0;
    for (int i = 0; i < inv_count; i++)
        player_add_item(p, &inv_items[i]);

    /* Update camera to loaded location */
    if (game->world) {
        Location *loc = world_get_location(game->world, p->current_location_id);
        if (loc) {
            game->camera.world_w = loc->room_width;
            game->camera.world_h = loc->room_height;
        }
    }
    camera_snap(&game->camera, p->x, p->y);

    return 1;
}

/* ── Legacy wrappers ─────────────────────────────────────────────────────── */

int savegame_exists(void)         { return savegame_exists_slot(1); }
int savegame_save(const Game *g)  { return savegame_save_slot(g, 1); }
int savegame_load(Game *g)        { return savegame_load_slot(g, 1); }
