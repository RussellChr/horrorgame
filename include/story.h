#ifndef STORY_H
#define STORY_H

#include "player.h"
#include "world.h"
#include "dialogue.h"

/* ── Endings ───────────────────────────────────────────────────────────── */

typedef enum {
    ENDING_NONE = 0,
    ENDING_ESCAPE,          /* Player escapes the house                  */
    ENDING_SACRIFICE,       /* Player sacrifices themselves              */
    ENDING_TRUTH,           /* Player uncovers the full truth            */
    ENDING_CORRUPTION,      /* Player succumbs to the horror             */
    ENDING_COUNT
} Ending;

/* ── Chapter IDs ───────────────────────────────────────────────────────── */

#define CHAPTER_PROLOGUE    0
#define CHAPTER_ONE         1
#define CHAPTER_TWO         2
#define CHAPTER_THREE       3
#define CHAPTER_FINALE      4
#define CHAPTER_COUNT       5

/* ── Story flags (single-bit bitmask values for Player.flags) ──────────── */
/* Pass these directly to player_set_flag / player_check_flag.             */

#define FLAG_FOUND_DIARY        (1u << 1)
#define FLAG_OPENED_BASEMENT    (1u << 2)
#define FLAG_SOLVED_PUZZLE      (1u << 3)
#define FLAG_KNOWS_TRUTH        (1u << 4)
#define FLAG_MONSTER_AWARE      (1u << 6)
#define FLAG_KEY_OBTAINED       (1u << 7)

/* ── StoryState ────────────────────────────────────────────────────────── */

typedef struct {
    int     current_chapter;
    Ending  ending;
    int     choices_made;   /* total number of choices made by the player */
    int     dark_choices;   /* choices that increased corruption          */
} StoryState;

/* ── API ───────────────────────────────────────────────────────────────── */

StoryState *story_create(void);
void        story_destroy(StoryState *state);

int  story_load(StoryState *state, const char *filepath);

void story_advance_chapter(StoryState *state, Player *player,
                           World *world);
void story_show_chapter_intro(const StoryState *state);

/* Evaluate which ending is reached based on flags / stats.               */
Ending story_determine_ending(const StoryState *state,
                               const Player *player);
void   story_show_ending(Ending ending);

/* Trigger a named story event; returns 1 if the event fires.            */
int story_trigger_event(StoryState *state, Player *player,
                        World *world, const char *event_name);

/* Populate the world with locations read from filepath.                  */
int story_populate_world(World *world, const char *filepath);

#endif /* STORY_H */
