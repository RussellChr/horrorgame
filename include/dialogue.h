#ifndef DIALOGUE_H
#define DIALOGUE_H

#include <SDL3/SDL.h>

#define MAX_DIALOGUE_NODES 128
#define MAX_CHOICES         8
#define NPC_NAME_MAX       64
#define DIALOGUE_TEXT_MAX  512

/* ── Dialogue choice ──────────────────────────────────────────────────── */

typedef struct {
    int  id;
    char text[DIALOGUE_TEXT_MAX];
    int  next_node_id;
    int  requires_courage;    /* minimum courage needed          */
    int  requires_item_id;    /* 0 = no item required            */
    int  sanity_delta;        /* change to sanity when chosen    */
    int  courage_delta;       /* change to courage when chosen   */
    int  story_flag;          /* flag to set on this choice      */
} DialogueChoice;

/* ── Dialogue node ────────────────────────────────────────────────────── */

typedef struct {
    int           id;
    char          speaker[NPC_NAME_MAX];
    char          text[DIALOGUE_TEXT_MAX];
    DialogueChoice choices[MAX_CHOICES];
    int           choice_count;
    int           is_terminal;
} DialogueNode;

/* ── Dialogue tree ────────────────────────────────────────────────────── */

typedef struct {
    DialogueNode nodes[MAX_DIALOGUE_NODES];
    int          node_count;
} DialogueTree;

/* ── Visual dialogue state ───────────────────────────────────────────── */

typedef struct {
    DialogueTree *tree;
    int           current_node_id;
    float         text_timer;       /* seconds accumulated           */
    int           chars_visible;    /* how many chars to show now    */
    int           text_complete;    /* 1 when all chars are shown    */
    int           selected_choice;  /* currently highlighted choice  */
    SDL_Texture   *bg_texture;      /* background texture (optional) */
} DialogueState;

/* ── Tree management ──────────────────────────────────────────────────── */

DialogueTree  *dialogue_tree_create(void);
void           dialogue_tree_destroy(DialogueTree *tree);

DialogueNode  *dialogue_get_node(DialogueTree *tree, int node_id);
DialogueNode  *dialogue_add_node(DialogueTree *tree, int id,
                                 const char *speaker, const char *text,
                                 int is_terminal);
void           dialogue_add_choice(DialogueNode *node,
                                   const DialogueChoice *choice);

/* Console output (for debugging / text mode) */
void dialogue_print_node(const DialogueNode *node,
                         int player_courage, int player_item_id);
int  dialogue_run(DialogueTree *tree, int start_node_id,
                  int player_courage, int player_item_id);

/* ── Visual dialogue API ─────────────────────────────────────────────── */

void dialogue_state_init(DialogueState *ds,
                         DialogueTree *tree, int start_node_id);

/* Update typewriter effect; dt in seconds. */
void dialogue_state_update(DialogueState *ds, float dt);

/* Advance to next node (or terminate); returns 0 when dialogue ends. */
int dialogue_state_advance(DialogueState *ds,
                           int player_courage, int player_item_id);

/* Get the currently selected choice (when text is complete). */
const DialogueChoice *dialogue_state_get_selected(const DialogueState *ds,
                                                   int player_courage,
                                                   int player_item_id);

/* Draw the visual dialogue box at the bottom of the screen. */
void dialogue_render(const DialogueState *ds,
                     SDL_Renderer *renderer,
                     int screen_w, int screen_h);

/* Texture management for dialogue background. */
void dialogue_load_texture(DialogueState *ds,
                           SDL_Renderer *renderer, const char *path);
void dialogue_unload_texture(DialogueState *ds);

/* Build a default dialogue tree for a given location / NPC. */
DialogueTree *dialogue_build_for_location(int location_id);

#endif /* DIALOGUE_H */
