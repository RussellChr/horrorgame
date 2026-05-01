#ifndef SAVEGAME_H
#define SAVEGAME_H

/* Pull in the full Game definition.
 * savegame.h is only ever included by translation units that build
 * the game (game.c, savegame.c), so there is no circular dependency. */
#include "game.h"

/* Path where the save file is written / read. */
#define SAVEGAME_PATH "savegame.sav"

/* Returns 1 if a save file exists and is readable, 0 otherwise. */
int savegame_exists(void);

/* Serialise the current game state to SAVEGAME_PATH.
 * Returns 1 on success, 0 on failure. */
int savegame_save(const Game *game);

/* Restore a previously saved state into a game that has already been
 * initialised via game_start_new().  Returns 1 on success, 0 on failure. */
int savegame_load(Game *game);

#endif /* SAVEGAME_H */
