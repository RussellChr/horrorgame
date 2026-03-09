#ifndef GAME_H
#define GAME_H

#include <SDL3/SDL.h>
#include "render.h"    /* WINDOW_W, WINDOW_H */
#include "camera.h"
#include "player.h"
#include "world.h"
#include "dialogue.h"
#include "story.h"
#include "ui.h"

/* ── Game states ──────────────────────────────────────────────────────── */

typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_PLAYING,
    GAME_STATE_DIALOGUE,
    GAME_STATE_INVENTORY,
    GAME_STATE_PAUSE,
    GAME_STATE_ENDING,
    GAME_STATE_QUIT
} GameState;

/* ── Game context ─────────────────────────────────────────────────────── */

typedef struct {
    /* SDL handles */
    SDL_Window   *window;
    SDL_Renderer *renderer;

    /* State machine */
    GameState     state;

    /* Subsystems */
    Player      *player;
    World       *world;
    StoryState  *story;
    DialogueTree  *dialogue_tree;   /* current NPC dialogue tree    */
    DialogueState  dialogue_state;  /* visual dialogue playback     */

    /* Camera */
    Camera camera;

    /* Near-interaction tracking */
    int  near_interactive;           /* 1 if near an interactive zone */
    int  interactive_trigger_id;     /* trigger_id of the nearby obj  */
    char interact_label[64];         /* text shown in the prompt      */

    /* Main menu */
    Button buttons[3];
    int    current_menu_choice;
    float  mouse_x, mouse_y;
    int    mouse_clicked;

    /* Pause menu */
    Button pause_buttons[2];
    int    pause_choice;

    /* Inventory */
    int selected_inventory_slot;

    /* Ending */
    int   ending_type;               /* Ending enum value            */
    float ending_timer;              /* seconds since ending started */

    /* Timing */
    Uint64 last_ticks;
    float  delta_time;

    /* Keyboard state (pointer into SDL's internal array) */
    const bool *keys;

    /* Running flag */
    int running;
} Game;

/* ── Lifecycle ────────────────────────────────────────────────────────── */

Game *game_init(SDL_Window *window, SDL_Renderer *renderer);
void  game_cleanup(Game *game);

/* ── Per-frame ────────────────────────────────────────────────────────── */

void game_handle_event(Game *game, SDL_Event *event);
void game_update(Game *game);
void game_render(Game *game);

/* ── State transitions ───────────────────────────────────────────────── */

void game_start_new(Game *game);
void game_change_location(Game *game, int location_id,
                          float spawn_x, float spawn_y);
void game_start_dialogue(Game *game, int node_id);
void game_end_dialogue(Game *game);
void game_trigger_ending(Game *game);

/* ── Per-state render helpers ────────────────────────────────────────── */

void game_render_menu(Game *game);
void game_render_playing(Game *game);
void game_render_dialogue_overlay(Game *game);
void game_render_inventory(Game *game);
void game_render_pause(Game *game);
void game_render_ending(Game *game);

#endif /* GAME_H */
