#ifndef STORY_H
#define STORY_H

#include "player.h"
#include "world.h"
#include "dialogue.h"

/* ── Chapter IDs ───────────────────────────────────────────────────────── */

#define CHAPTER_PROLOGUE    0

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

#endif /* STORY_H */
