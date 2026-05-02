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

    if (player_check_flag(player, FLAG_KEYCARD_COLLECTED))
        return "Use the keycard to unlock the archive";

    if (player_check_flag(player, FLAG_SECURITY_PASSCODE_DONE))
        return "Find the keycard in the lab";

    if (player_check_flag(player, FLAG_POWER_GENERATOR_ON))
        return "Find the security access code";

    if (player_check_flag(player, FLAG_HIBERN_POWERCELL_PLACED))
        return "Restore power to the facility";

    return "Find a way to open the door";
}
