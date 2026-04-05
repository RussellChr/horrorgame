#include "interactions.h"
#include "game.h"
#include "player.h"
#include "world.h"
#include "dialogue.h"
#include "monologue.h"
#include "story.h"

#include <string.h>

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* Set game->dialogue_tree: try the named monologue section first; fall back
 * to the location-based procedural tree when no monologue is found. */
void game_set_dialogue_tree(Game *game,
                             const char *monologue_id,
                             int fallback_location_id)
{
    if (monologue_id) {
        const MonologueSection *ms =
            monologue_find(&game->monologue_file, monologue_id);
        if (ms) {
            game->dialogue_tree = monologue_to_dialogue_tree(ms);
            if (game->dialogue_tree) return;
        }
    }
    game->dialogue_tree = dialogue_build_for_location(fallback_location_id);
}

/* Apply story flag from the currently selected dialogue choice. */
void game_apply_dialogue_choice_flag(Game *game)
{
    if (!game || !game->player || !game->dialogue_state.text_complete) return;

    const DialogueChoice *ch = dialogue_state_get_selected(
        &game->dialogue_state, 0);
    if (!ch) return;

    if (ch->story_flag)
        game->player->flags |= (uint32_t)ch->story_flag;
}

/* ── Interaction handler ─────────────────────────────────────────────────── */

void game_handle_interaction(Game *game)
{
    if (!game || !game->player || !game->world) return;

    int loc_id = game->player->current_location_id;
    int tid    = game->interactive_trigger_id;

    /* Guard: don't open duplicate dialogue */
    if (game->dialogue_tree) return;

    /* Locker interaction (Hallway, trigger 60) */
    if (tid == 60 && loc_id == 2) {
        game->state = GAME_STATE_LOCKER;
        return;
    }

    /* ── Lab interactions (loc 1) ──────────────────────────────────────── */
    if (loc_id == 1) {
        if (tid == 61) {
            /* Tile 2: keycard pickup */
            if (!player_check_flag(game->player, FLAG_KEYCARD_COLLECTED)) {
                player_set_flag(game->player, FLAG_KEYCARD_COLLECTED);
                Item kc;
                strncpy(kc.name, "Keycard", ITEM_NAME_MAX - 1);
                kc.name[ITEM_NAME_MAX - 1] = '\0';
                strncpy(kc.description, "A keycard that grants access to the archive.", ITEM_DESC_MAX - 1);
                kc.description[ITEM_DESC_MAX - 1] = '\0';
                kc.id     = ITEM_ID_KEYCARD;
                kc.usable = 0;
                player_add_item(game->player, &kc);
                game_set_dialogue_tree(game, "lab_keycard", 1);
            } else {
                game_set_dialogue_tree(game, "hallway_nothing", 1);
            }
        }
        if (game->dialogue_tree)
            game_start_dialogue(game, 0);
        return;
    }

    /* ── Hallway interactions (loc 2) ──────────────────────────────────── */
    if (loc_id == 2) {
        if (tid == 80) {
            /* Tile 8: interactable – nothing here */
            game_set_dialogue_tree(game, "hallway_nothing", 2);
        } else if (tid == 81) {
            /* Tile 9: gas mask pickup */
            if (!player_check_flag(game->player, FLAG_HALLWAY_GASMASK_COLLECTED)) {
                player_set_flag(game->player, FLAG_HALLWAY_GASMASK_COLLECTED);
                Item gm;
                strncpy(gm.name, "Gas Mask", ITEM_NAME_MAX - 1);
                gm.name[ITEM_NAME_MAX - 1] = '\0';
                strncpy(gm.description, "Gas mask found.", ITEM_DESC_MAX - 1);
                gm.description[ITEM_DESC_MAX - 1] = '\0';
                gm.id     = ITEM_ID_GASMASK;
                gm.usable = 1;
                player_add_item(game->player, &gm);
                game_set_dialogue_tree(game, "hallway_gasmask", 2);
            } else {
                game_set_dialogue_tree(game, "hallway_nothing", 2);
            }
        } else if (tid == 82) {
            /* Tile 10: flashlight pickup */
            if (!player_check_flag(game->player, FLAG_HALLWAY_FLASHLIGHT_COLLECTED)) {
                player_set_flag(game->player, FLAG_HALLWAY_FLASHLIGHT_COLLECTED);
                Item fl;
                strncpy(fl.name, "Flashlight", ITEM_NAME_MAX - 1);
                fl.name[ITEM_NAME_MAX - 1] = '\0';
                strncpy(fl.description, "Flashlight found.", ITEM_DESC_MAX - 1);
                fl.description[ITEM_DESC_MAX - 1] = '\0';
                fl.id     = ITEM_ID_FLASHLIGHT;
                fl.usable = 1;
                player_add_item(game->player, &fl);
                game_set_dialogue_tree(game, "hallway_flashlight", 2);
            } else {
                game_set_dialogue_tree(game, "hallway_nothing", 2);
            }
        } else if (tid == 95) {
            /* Tile 5: archive door – locked until player has keycard */
            if (!player_check_flag(game->player, FLAG_ARCHIVE_UNLOCKED) &&
                player_has_item(game->player, ITEM_ID_KEYCARD)) {
                /* Unlock the archive door permanently: remove door colliders
                   and convert the interactive trigger into an exit trigger
                   (trigger_id = -1 signals a room-transition trigger). */
                player_set_flag(game->player, FLAG_ARCHIVE_UNLOCKED);
                Location *hwloc = world_get_location(game->world, 2);
                if (hwloc) {
                    hwloc->collider_count = hwloc->door_collider_start;
                    for (int i = 0; i < hwloc->trigger_count; i++) {
                        if (hwloc->triggers[i].trigger_id == 95) {
                            hwloc->triggers[i].target_location_id = 0;
                            hwloc->triggers[i].trigger_id = -1;
                        }
                    }
                }
                /* Transport player to the archive immediately. */
                Location *aloc = world_get_location(game->world, 0);
                if (aloc)
                    game_change_location(game, 0, aloc->spawn_x, aloc->spawn_y);
                return;
            } else if (!player_check_flag(game->player, FLAG_ARCHIVE_UNLOCKED)) {
                game_set_dialogue_tree(game, "archive_door_locked", 2);
            }
        }
        if (game->dialogue_tree)
            game_start_dialogue(game, 0);
        return;
    }

    /* ── Hibernation room interactions (loc 3) ─────────────────────────── */
    if (loc_id == 3) {
        if (tid == 71) {
            /* Tile 1: zonk – nothing here */
            game_set_dialogue_tree(game, "hibern_zonk", 3);
        } else if (tid == 72) {
            /* Tile 2: power cell pickup */
            if (!player_check_flag(game->player, FLAG_HIBERN_POWERCELL_COLLECTED)) {
                player_set_flag(game->player, FLAG_HIBERN_POWERCELL_COLLECTED);
                Item pc;
                strncpy(pc.name, "Power Cell", ITEM_NAME_MAX - 1);
                pc.name[ITEM_NAME_MAX - 1] = '\0';
                strncpy(pc.description, "A cylindrical power cell.", ITEM_DESC_MAX - 1);
                pc.description[ITEM_DESC_MAX - 1] = '\0';
                pc.id     = ITEM_ID_POWERCELL;
                pc.usable = 0;
                player_add_item(game->player, &pc);
                game_set_dialogue_tree(game, "hibern_powercell", 3);
            } else {
                game_set_dialogue_tree(game, "hibern_zonk", 3);
            }
        } else if (tid == 73) {
            /* Tile 3: power cell slot */
            if (player_has_item(game->player, ITEM_ID_POWERCELL)) {
                player_remove_item(game->player, ITEM_ID_POWERCELL);
                player_set_flag(game->player, FLAG_HIBERN_POWERCELL_PLACED);

                /* Open the door: remove tile-5 collision barrier and convert
                   the tile-5 interactive triggers into real exit triggers so
                   the player can walk through to the hallway.
                   Note: door colliders are always the last ones added in
                   world_setup_rooms (after map_build_colliders and before any
                   trigger registration), so truncating to door_collider_start
                   removes exactly and only the door colliders. */
                Location *hloc = world_get_location(game->world, 3);
                if (hloc) {
                    hloc->collider_count = hloc->door_collider_start;
                    for (int i = 0; i < hloc->trigger_count; i++) {
                        if (hloc->triggers[i].trigger_id == 75) {
                            hloc->triggers[i].target_location_id = 2;
                        }
                    }
                }

                game_set_dialogue_tree(game, "hibern_slot_place", 3);
            } else {
                game_set_dialogue_tree(game, "hibern_slot_empty", 3);
            }
        } else if (tid == 74) {
            /* Tile 4: flavor description – accessible any time */
            game_set_dialogue_tree(game, "hibern_pods_opened", 3);
        } else if (tid == 75) {
            /* Tile 5: door – still interactive = still locked */
            game_set_dialogue_tree(game, "hibern_door_locked", 3);
        }
        if (game->dialogue_tree)
            game_start_dialogue(game, 0);
        return;
    }

    /* ── Power room interactions (loc 4) ───────────────────────────────── */
    if (loc_id == 4) {
        if (tid == 101) {
            /* Tile 1: fuel tank – can be picked up twice */
            if (!player_check_flag(game->player, FLAG_POWER_FUELTANK1_COLLECTED)) {
                player_set_flag(game->player, FLAG_POWER_FUELTANK1_COLLECTED);
                Item ft;
                strncpy(ft.name, "Fuel Tank", ITEM_NAME_MAX - 1);
                ft.name[ITEM_NAME_MAX - 1] = '\0';
                strncpy(ft.description, "A heavy fuel tank.", ITEM_DESC_MAX - 1);
                ft.description[ITEM_DESC_MAX - 1] = '\0';
                ft.id     = ITEM_ID_FUELTANK;
                ft.usable = 0;
                player_add_item(game->player, &ft);
                game_set_dialogue_tree(game, "power_fueltank", 4);
            } else if (!player_check_flag(game->player, FLAG_POWER_FUELTANK2_COLLECTED) &&
                        player_check_flag(game->player, FLAG_POWER_FUELTANK1_PLACED)) {
                /* Second pickup: only available after first tank has been placed */
                player_set_flag(game->player, FLAG_POWER_FUELTANK2_COLLECTED);
                Item ft;
                strncpy(ft.name, "Fuel Tank", ITEM_NAME_MAX - 1);
                ft.name[ITEM_NAME_MAX - 1] = '\0';
                strncpy(ft.description, "A heavy fuel tank.", ITEM_DESC_MAX - 1);
                ft.description[ITEM_DESC_MAX - 1] = '\0';
                ft.id     = ITEM_ID_FUELTANK;
                ft.usable = 0;
                player_add_item(game->player, &ft);
                game_set_dialogue_tree(game, "power_fueltank2", 4);
            } else {
                game_set_dialogue_tree(game, "power_fueltank_nothing", 4);
            }
        } else if (tid == 102) {
            /* Tile 2: first fuel slot */
            if (player_has_item(game->player, ITEM_ID_FUELTANK) &&
                !player_check_flag(game->player, FLAG_POWER_FUELTANK1_PLACED)) {
                player_remove_item(game->player, ITEM_ID_FUELTANK);
                player_set_flag(game->player, FLAG_POWER_FUELTANK1_PLACED);
                game_set_dialogue_tree(game, "power_fueltank_placed", 4);
            } else if (!player_check_flag(game->player, FLAG_POWER_FUELTANK1_PLACED)) {
                game_set_dialogue_tree(game, "power_slot_empty", 4);
            } else {
                game_set_dialogue_tree(game, "power_slot_done", 4);
            }
        } else if (tid == 103) {
            /* Tile 3: valve A – only turnable after fuel slot A is filled */
            if (!player_check_flag(game->player, FLAG_POWER_FUELTANK1_PLACED)) {
                game_set_dialogue_tree(game, "power_valve_locked", 4);
            } else if (!player_check_flag(game->player, FLAG_POWER_VALVE1_OPENED)) {
                player_set_flag(game->player, FLAG_POWER_VALVE1_OPENED);
                game_set_dialogue_tree(game, "power_valve_opened", 4);
            } else {
                game_set_dialogue_tree(game, "power_valve_done", 4);
            }
        } else if (tid == 104) {
            /* Tile 4: generator – starts Simon game once both valves are open */
            if (!player_check_flag(game->player, FLAG_POWER_VALVE1_OPENED) ||
                !player_check_flag(game->player, FLAG_POWER_VALVE2_OPENED)) {
                game_set_dialogue_tree(game, "power_generator_locked", 4);
            } else if (!player_check_flag(game->player, FLAG_POWER_GENERATOR_ON)) {
                /* Start the Simon minigame – no dialogue, switch state directly */
                game_start_simon(game);
                return;
            } else {
                game_set_dialogue_tree(game, "power_generator_done", 4);
            }
        } else if (tid == 106) {
            /* Tile 6: second fuel slot – requires first slot to be filled first */
            if (!player_check_flag(game->player, FLAG_POWER_FUELTANK1_PLACED)) {
                game_set_dialogue_tree(game, "power_slot2_locked", 4);
            } else if (player_has_item(game->player, ITEM_ID_FUELTANK) &&
                !player_check_flag(game->player, FLAG_POWER_FUELTANK2_PLACED)) {
                player_remove_item(game->player, ITEM_ID_FUELTANK);
                player_set_flag(game->player, FLAG_POWER_FUELTANK2_PLACED);
                game_set_dialogue_tree(game, "power_fueltank_placed2", 4);
            } else if (!player_check_flag(game->player, FLAG_POWER_FUELTANK2_PLACED)) {
                game_set_dialogue_tree(game, "power_slot_empty", 4);
            } else {
                game_set_dialogue_tree(game, "power_slot_done", 4);
            }
        } else if (tid == 107) {
            /* Tile 7: valve B – only turnable after fuel slot B is filled */
            if (!player_check_flag(game->player, FLAG_POWER_FUELTANK2_PLACED)) {
                game_set_dialogue_tree(game, "power_valve_locked", 4);
            } else if (!player_check_flag(game->player, FLAG_POWER_VALVE2_OPENED)) {
                player_set_flag(game->player, FLAG_POWER_VALVE2_OPENED);
                game_set_dialogue_tree(game, "power_valve_opened", 4);
            } else {
                game_set_dialogue_tree(game, "power_valve_done", 4);
            }
        }
        if (game->dialogue_tree)
            game_start_dialogue(game, 0);
        return;
    }

    /* Portrait interaction (Entrance Hall, trigger 30) */
    if (tid == 30 && loc_id == 0) {
        game_set_dialogue_tree(game, "portrait", 30);
    }
    /* Stranger NPC interaction (Entrance Hall, trigger 40) */
    else if (tid == 40 && loc_id == 0) {
        game_set_dialogue_tree(game, "stranger", 40);
    }
    /* ── Security room interactions (loc 5) ────────────────────────────── */
    else if (loc_id == 5) {
        if (tid == 91) {
            /* Tile 2: readable note – Yes/No prompt */
            DialogueTree *tree = dialogue_tree_create();
            if (tree) {
                DialogueNode *q = dialogue_add_node(tree, 0, "",
                    "Would you like to read it?", 0);
                if (q) {
                    DialogueChoice yes;
                    memset(&yes, 0, sizeof(yes));
                    yes.id           = 0;
                    yes.next_node_id = -1; /* sentinel: detected in event handler to end dialogue */
                    yes.story_flag   = FLAG_SECURITY_NOTE_READ;
                    strncpy(yes.text, "Yes", DIALOGUE_TEXT_MAX - 1);
                    dialogue_add_choice(q, &yes);

                    DialogueChoice no;
                    memset(&no, 0, sizeof(no));
                    no.id           = 1;
                    no.next_node_id = -1; /* sentinel: detected in event handler to end dialogue */
                    no.story_flag   = 0;
                    strncpy(no.text, "No", DIALOGUE_TEXT_MAX - 1);
                    dialogue_add_choice(q, &no);
                }
                game->dialogue_tree = tree;
                game_start_dialogue(game, 0);
            }
        } else if (tid == 92) {
            /* Tile 4: monitor screen – show zoomed image */
            game->show_monitor_zoom = 1;
            game->state = GAME_STATE_LOCKER;
        }
        return;
    }
    /* Default interaction */
    else {
        game->dialogue_tree = dialogue_build_for_location(loc_id);
    }

    if (game->dialogue_tree)
        game_start_dialogue(game, 0);
}
