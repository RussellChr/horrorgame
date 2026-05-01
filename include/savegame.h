#ifndef SAVEGAME_H
#define SAVEGAME_H

#include "player.h"

/* ── Save-file constants ───────────────────────────────────────────────── */

#define SAVE_SLOT_COUNT  3
#define SAVE_MAGIC       "HGSAVE1"   /* 7 chars + implicit NUL fills magic[8] */
#define SAVE_LABEL_MAX   48          /* max length of the human-readable label */

/* ── SaveData ──────────────────────────────────────────────────────────── */

typedef struct {
    char     magic[8];          /* "HGSAVE1\0" – used to validate the file   */
    uint32_t player_flags;      /* all story/progress flags                  */
    float    player_x;
    float    player_y;
    int      location_id;
    int      inventory_count;
    Item     inventory[INVENTORY_CAPACITY];
    int      current_chapter;
    int      choices_made;
    int      enemy_active;      /* 1 if the enemy is patrolling              */
    int      flashlight_active;
    int      gasmask_active;
    int      security_cutscene_played;
    char     save_label[SAVE_LABEL_MAX]; /* e.g. "Slot 1 – Hallway"         */
} SaveData;

/* ── API ───────────────────────────────────────────────────────────────── */

/* Return the file path for a save slot (1-based).  Pointer is static. */
const char *savegame_slot_path(int slot);

/* Return 1 if a valid save file exists for slot (1-based), else 0. */
int  savegame_exists(int slot);

/* Read only the label field for slot into label_out (at most label_max bytes).
 * Returns 1 on success, 0 if the slot has no valid save. */
int  savegame_read_label(int slot, char *label_out, int label_max);

/* Write a complete SaveData to slot.  Returns 1 on success, 0 on error. */
int  savegame_write(int slot, const SaveData *data);

/* Read a complete SaveData from slot.  Returns 1 on success, 0 on error. */
int  savegame_read(int slot, SaveData *data);

#endif /* SAVEGAME_H */
