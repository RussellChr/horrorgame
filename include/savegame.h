#ifndef SAVEGAME_H
#define SAVEGAME_H

/* Pull in the full Game definition. */
#include "game.h"

/* ── Multiple save slots ──────────────────────────────────────────────── */
#define SAVE_SLOT_COUNT  3   /* number of independent save slots        */

/* Fill buf with the file-system path for the given slot (1-based). */
void savegame_slot_path(int slot, char *buf, int buf_size);

/* Returns 1 if the given slot has a save file, 0 otherwise. */
int savegame_exists_slot(int slot);

/* Fill buf with a short human-readable description of the save in the
 * given slot (e.g. "Hallway", "Archive").  Writes "Empty" when the slot
 * has no save file.  buf is always null-terminated. */
void savegame_get_slot_info(int slot, char *buf, int buf_size);

/* Write the current game state to the given slot.
 * Returns 1 on success, 0 on failure. */
int savegame_save_slot(const Game *game, int slot);

/* Restore a previously saved state (slot) into a game already initialised
 * via game_start_new().  Returns 1 on success, 0 on failure. */
int savegame_load_slot(Game *game, int slot);

/* ── Legacy single-file wrappers (kept for ABI stability) ──────────────── */
/* These redirect to slot 1. */
int savegame_exists(void);
int savegame_save(const Game *game);
int savegame_load(Game *game);

#endif /* SAVEGAME_H */
