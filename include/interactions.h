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

/* ── Trigger IDs ─────────────────────────────────────────────────────────
 *
 * Each interactive trigger zone in a CSV map is assigned a unique ID here.
 * The IDs are used in game_handle_interaction to dispatch per-trigger logic.
 */

/* Hallway / general */
#define TRIGGER_LOCKER             60   /* Hallway locker               */
#define TRIGGER_LAB_KEYCARD        61   /* Lab keycard pickup           */
#define TRIGGER_HALLWAY_NOTHING    80   /* Hallway tile 8 (empty)       */
#define TRIGGER_HALLWAY_GASMASK    81   /* Hallway gas mask pickup      */
#define TRIGGER_HALLWAY_FLASHLIGHT 82   /* Hallway flashlight pickup    */
#define TRIGGER_HALLWAY_ARCHIVE_DOOR 95 /* Hallway → archive door       */

/* Security room */
#define TRIGGER_SECURITY_MONITOR   92   /* Security monitor zoom        */
#define TRIGGER_SECURITY_NOTE      91   /* Security readable note       */

/* Hibernation room */
#define TRIGGER_HIBERN_ZONK        71   /* Hibernation tile 1 (empty)   */
#define TRIGGER_HIBERN_POWERCELL   72   /* Hibernation power cell       */
#define TRIGGER_HIBERN_SLOT        73   /* Hibernation power cell slot  */
#define TRIGGER_HIBERN_PODS        74   /* Hibernation pods             */
#define TRIGGER_HIBERN_DOOR        75   /* Hibernation door             */

/* Power room */
#define TRIGGER_POWER_FUELTANK     101  /* Power room fuel tank         */
#define TRIGGER_POWER_SLOT1        102  /* Power room fuel slot 1       */
#define TRIGGER_POWER_VALVE1       103  /* Power room valve A           */
#define TRIGGER_POWER_GENERATOR    104  /* Power room generator         */
#define TRIGGER_POWER_SLOT2        106  /* Power room fuel slot 2       */
#define TRIGGER_POWER_VALVE2       107  /* Power room valve B           */

/* Archive room (loc 0) */
#define TRIGGER_ARCHIVE_INNER_DOOR  110  /* Archive inner door (tile 2) */
#define TRIGGER_ARCHIVE_FINGERPRINT 111  /* Archive fingerprint (tile 3) */
#define TRIGGER_ARCHIVE_THERMALFUSE 112  /* Archive thermal fuse (tile 4) */

/* Entrance Hall */
#define TRIGGER_ENTRANCE_PORTRAIT   30  /* Portrait                     */
#define TRIGGER_ENTRANCE_STRANGER   40  /* Stranger NPC                 */

void game_handle_interaction(Game *game);
void game_set_dialogue_tree(Game *game,
                             const char *monologue_id,
                             int fallback_location_id);
void game_apply_dialogue_choice_flag(Game *game);

#endif /* INTERACTIONS_H */
