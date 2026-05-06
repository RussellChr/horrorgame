#include "story.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Chapter introduction texts ────────────────────────────────────────── */

static const char *prologue_intro =
    "The rain has not stopped for three days. You stand before the old\n"
    "Ashwood estate, your car broken down a mile behind you. The house\n"
    "looms ahead – the only shelter for miles.";

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

StoryState *story_create(void) {
    StoryState *s = calloc(1, sizeof(StoryState));
    if (s) s->current_chapter = CHAPTER_PROLOGUE;
    return s;
}

void story_destroy(StoryState *state) {
    free(state);
}

/* ── Chapter introduction ───────────────────────────────────────────────── */

void story_show_chapter_intro(const StoryState *state) {
    if (!state) return;

    printf("\n══════════════════════════════════════════\n");
    printf("              PROLOGUE\n");
    printf("══════════════════════════════════════════\n\n");
    utils_print_slow(prologue_intro, 20);
    printf("\n");
    utils_press_enter();
}

/* ── World population ───────────────────────────────────────────────────── */

int story_populate_world(World *world, const char *filepath) {
    return world_load_locations(world, filepath);
}

/* ── File loading (stub for future save/load) ──────────────────────────── */

int story_load(StoryState *state, const char *filepath) {
    (void)state; (void)filepath;
    /* Placeholder: full save/load not yet implemented */
    return 0;
}

/* ── Objective text ─────────────────────────────────────────────────────── */

const char *story_get_objective(const Player *player)
{
    if (!player) return "Find a way to open the door";

    /* Step 9 – escape */
    if (player_check_flag(player, FLAG_ARCHIVE_KEYCARD2_COLLECTED))
        return "Use the Level-2 Keycard to reach the exit";

    /* Step 8 – inner archive door open, find the lvl-2 keycard */
    if (player_check_flag(player, FLAG_ARCHIVE_INNER_DOOR_OPENED))
        return "Find the Level-2 Keycard in the archive";

    /* Step 7 – inside archive, open the inner door */
    if (player_check_flag(player, FLAG_ARCHIVE_UNLOCKED))
        return "Find a way to open the inner archive door";

    /* Step 6 – have keycard, go unlock the archive */
    if (player_check_flag(player, FLAG_KEYCARD_COLLECTED))
        return "Use the keycard to unlock the archive";

    /* Step 5 – passcode done, go find keycard in lab */
    if (player_check_flag(player, FLAG_SECURITY_PASSCODE_DONE))
        return "Find the keycard in the lab";

    /* Step 4 – power on, go to security room to find the access code */
    if (player_check_flag(player, FLAG_POWER_GENERATOR_ON))
        return "Find the security access code";

    /* Step 3b – both fuel tanks placed, open valves and start generator */
    if (player_check_flag(player, FLAG_POWER_FUELTANK2_PLACED))
        return "Open both valves and start the generator";

    /* Step 3a – first fuel placed, collect and place second tank */
    if (player_check_flag(player, FLAG_POWER_FUELTANK1_PLACED))
        return "Place the second fuel tank and open the valves";

    /* Step 2 – fuel collected, place it in the power room */
    if (player_check_flag(player, FLAG_POWER_FUELTANK1_COLLECTED))
        return "Place the fuel tanks in the power room slots";

    /* Step 1 – door unlocked, head to the power room */
    if (player_check_flag(player, FLAG_HIBERN_POWERCELL_PLACED))
        return "Go to the power room and restore power to the facility";

    /* Step 0 – starting state, find the power cell in Hibernation */
    if (player_check_flag(player, FLAG_HIBERN_POWERCELL_COLLECTED))
        return "Insert the power cell into the slot to open the door";

    return "Find something to power the door";
}
