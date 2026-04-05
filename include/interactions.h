#ifndef INTERACTIONS_H
#define INTERACTIONS_H

#include "game.h"

/* ── In-world interaction handler ───────────────────────────────────────
 *
 * game_handle_interaction  – called when the player presses E near an
 *                             interactive trigger; dispatches to the
 *                             correct per-room / per-trigger logic.
 *
 * game_set_dialogue_tree   – tries to build a dialogue tree from the named
 *                             monologue section first; falls back to the
 *                             location-based procedural tree.  Stores the
 *                             result in game->dialogue_tree.
 *
 * game_apply_dialogue_choice_flag – reads the currently selected dialogue
 *                                    choice and applies its story flag to
 *                                    the player's flag set.
 */

void game_handle_interaction(Game *game);
void game_set_dialogue_tree(Game *game,
                             const char *monologue_id,
                             int fallback_location_id);
void game_apply_dialogue_choice_flag(Game *game);

#endif /* INTERACTIONS_H */
