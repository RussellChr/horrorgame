#ifndef STORY_H
#define STORY_H

#include "player.h"
#include "world.h"
#include "dialogue.h"

/* ── Chapter IDs ───────────────────────────────────────────────────────── */

#define CHAPTER_PROLOGUE    0

/* ── Location IDs ──────────────────────────────────────────────────────── */

#define LOCATION_ARCHIVE    0   /* starting room – archive / entrance hall */
#define LOCATION_LAB        1   /* chemistry laboratory                    */
#define LOCATION_HALLWAY    2   /* corridor / hallway                      */

/* ── Story flags (bitmask values for player->flags) ───────────────────── */

#define FLAG_ENTERED_LAB                (1u << 0)   /* player visited the lab             */
#define FLAG_ENTERED_HALLWAY            (1u << 1)   /* player visited the hallway         */
#define FLAG_HIBERN_POWERCELL_COLLECTED (1u << 2)   /* power cell picked up in Hibernation */
#define FLAG_HIBERN_POWERCELL_PLACED    (1u << 3)   /* power cell placed in slot (door unlocked) */
#define FLAG_HALLWAY_GASMASK_COLLECTED  (1u << 4)   /* gas mask picked up in Hallway       */
#define FLAG_HALLWAY_FLASHLIGHT_COLLECTED (1u << 5) /* flashlight picked up in Hallway     */
#define FLAG_SECURITY_NOTE_READ           (1u << 6) /* player chose to read the note in Security room (transient) */

/* ── StoryState ────────────────────────────────────────────────────────── */

typedef struct {
    int current_chapter;
    int choices_made;   /* total number of choices made by the player */
} StoryState;

/* ── API ───────────────────────────────────────────────────────────────── */

StoryState *story_create(void);
void        story_destroy(StoryState *state);

int  story_load(StoryState *state, const char *filepath);

void story_show_chapter_intro(const StoryState *state);

/* Populate the world with locations read from filepath.                  */
int story_populate_world(World *world, const char *filepath);

/* Return the current objective string based on the player's story flags. */
const char *story_get_objective(const Player *player);

#endif /* STORY_H */
