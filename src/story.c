#include "story.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Chapter introduction texts ────────────────────────────────────────── */

static const char *chapter_intros[CHAPTER_COUNT] = {
    /* PROLOGUE */
    "The rain has not stopped for three days. You stand before the old\n"
    "Ashwood estate, your car broken down a mile behind you. The house\n"
    "looms ahead – the only shelter for miles.",

    /* CHAPTER ONE */
    "Inside the house you find dust, shadows, and something else entirely.\n"
    "A soft sound echoes from deeper within: a child humming a lullaby.",

    /* CHAPTER TWO */
    "The basement door has been opened. Cold air and old secrets rise\n"
    "from below. Whatever was kept down there has been waiting a long time.",

    /* CHAPTER THREE */
    "The truth begins to unravel. Pages torn from a diary reveal a history\n"
    "of rituals, disappearances, and a creature that feeds on forgetting.",

    /* FINALE */
    "Everything converges. The choices you have made define what happens\n"
    "next – and whether anyone leaves Ashwood alive."
};

/* ── Ending texts ───────────────────────────────────────────────────────── */

static const char *ending_texts[ENDING_COUNT] = {
    /* ENDING_NONE */
    "(No ending reached.)",

    /* ENDING_ESCAPE */
    "You sprint through the front door as dawn breaks.\n"
    "The estate collapses behind you in a roar of rotting timber.\n"
    "You do not look back. Some horrors are best left in the dark.\n\n"
    "              ── ENDING: ESCAPE ──\n",

    /* ENDING_SACRIFICE */
    "You face the creature alone, buying time for any chance of escape.\n"
    "The last thing you feel is a strange peace – you chose this.\n"
    "By morning the house is silent.\n\n"
    "              ── ENDING: SACRIFICE ──\n",

    /* ENDING_TRUTH */
    "You speak the creature's true name aloud. The house shudders.\n"
    "Light floods every shadow. The thing unravels, screaming.\n"
    "You have broken a curse older than the house itself.\n\n"
    "              ── ENDING: TRUTH ──\n",

    /* ENDING_CORRUPTION */
    "The darkness was patient. It waited for you to look too long,\n"
    "ask too much, want too deeply. By the time you realise what you\n"
    "have become, there is no one left – and neither are you.\n\n"
    "              ── ENDING: CORRUPTION ──\n"
};

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

StoryState *story_create(void) {
    StoryState *s = calloc(1, sizeof(StoryState));
    if (s) s->current_chapter = CHAPTER_PROLOGUE;
    return s;
}

void story_destroy(StoryState *state) {
    free(state);
}

/* ── Chapter progression ───────────────────────────────────────────────── */

void story_show_chapter_intro(const StoryState *state) {
    if (!state) return;
    int ch = state->current_chapter;
    if (ch < 0 || ch >= CHAPTER_COUNT) return;

    printf("\n══════════════════════════════════════════\n");
    if (ch == CHAPTER_PROLOGUE)
        printf("              PROLOGUE\n");
    else
        printf("              CHAPTER %d\n", ch);
    printf("══════════════════════════════════════════\n\n");
    utils_print_slow(chapter_intros[ch], 20);
    printf("\n");
    utils_press_enter();
}

void story_advance_chapter(StoryState *state, Player *player,
                           World *world) {
    (void)player; (void)world; /* may be used for future events */
    if (!state) return;
    if (state->current_chapter < CHAPTER_FINALE)
        state->current_chapter++;
}

/* ── Ending logic ───────────────────────────────────────────────────────── */

Ending story_determine_ending(const StoryState *state,
                               const Player *player) {
    if (!state || !player) return ENDING_NONE;

    /* Corruption: dark_choices > 5 */
    if (state->dark_choices > 5)
        return ENDING_CORRUPTION;

    /* Truth: player knows the truth and the puzzle is solved */
    if (player_check_flag(player, FLAG_KNOWS_TRUTH) &&
        player_check_flag(player, FLAG_SOLVED_PUZZLE))
        return ENDING_TRUTH;

    /* Sacrifice: monster is aware */
    if (player_check_flag(player, FLAG_MONSTER_AWARE))
        return ENDING_SACRIFICE;

    /* Default: escape */
    return ENDING_ESCAPE;
}

void story_show_ending(Ending ending) {
    if (ending < 0 || ending >= ENDING_COUNT) return;
    printf("\n══════════════════════════════════════════\n");
    utils_print_slow(ending_texts[ending], 25);
    printf("══════════════════════════════════════════\n\n");
}

/* ── Story events ───────────────────────────────────────────────────────── */

int story_trigger_event(StoryState *state, Player *player,
                        World *world, const char *event_name) {
    (void)world;
    if (!state || !player || !event_name) return 0;

    if (strcmp(event_name, "find_diary") == 0) {
        if (player_check_flag(player, FLAG_FOUND_DIARY)) return 0;
        player_set_flag(player, FLAG_FOUND_DIARY);
        printf("You find a water-stained diary beneath the floorboards.\n"
               "The last entry is dated ten years ago today.\n");
        state->choices_made++;
        return 1;
    }

    if (strcmp(event_name, "open_basement") == 0) {
        if (player_check_flag(player, FLAG_OPENED_BASEMENT)) return 0;
        if (!player_check_flag(player, FLAG_KEY_OBTAINED)) {
            printf("The basement door is locked tight.\n");
            return 0;
        }
        player_set_flag(player, FLAG_OPENED_BASEMENT);
        printf("The door groans open. Cold air rushes up from the dark below.\n");
        return 1;
    }

    /* Set when the player reads both the diary and the ritual book. */
    if (strcmp(event_name, "learn_truth") == 0) {
        if (player_check_flag(player, FLAG_KNOWS_TRUTH)) return 0;
        if (!player_check_flag(player, FLAG_FOUND_DIARY) ||
            !player_check_flag(player, FLAG_SOLVED_PUZZLE)) {
            printf("You need to read the diary and the ritual book first.\n");
            return 0;
        }
        player_set_flag(player, FLAG_KNOWS_TRUTH);
        printf("The terrible truth clicks into place.\n"
               "Professor Ashwood made a bargain with something ancient.\n"
               "But a bargain can be broken – if you are willing to pay.\n");
        state->choices_made++;
        return 1;
    }

    /* Set when the player examines the creature's signs in the ritual room. */
    if (strcmp(event_name, "study_creature") == 0) {
        if (player_check_flag(player, FLAG_MONSTER_AWARE)) return 0;
        if (!player_check_flag(player, FLAG_OPENED_BASEMENT)) {
            printf("You have not yet been to the basement.\n");
            return 0;
        }
        player_set_flag(player, FLAG_MONSTER_AWARE);
        printf("You study the carvings. The creature has a name – and names\n"
               "are weaknesses. This knowledge is terrifying, but also power.\n");
        return 1;
    }

    /* Set when the ritual circle puzzle is completed. */
    if (strcmp(event_name, "solve_puzzle") == 0) {
        if (player_check_flag(player, FLAG_SOLVED_PUZZLE)) return 0;
        player_set_flag(player, FLAG_SOLVED_PUZZLE);
        printf("The symbols on the floor align. The circle pulses with\n"
               "cold light. Something stirs beneath the stone.\n");
        return 1;
    }

    return 0;
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
