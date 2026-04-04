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
#include "npc.h"
#include "monologue.h"

/* ── Game states ──────────────────────────────────────────────────────── */

typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_PLAYING,
    GAME_STATE_DIALOGUE,
    GAME_STATE_INVENTORY,
    GAME_STATE_PAUSE,
    GAME_STATE_SETTINGS,
    GAME_STATE_LOCKER,
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
    Player        *player;
    World         *world;
    StoryState    *story;
    DialogueTree  *dialogue_tree;   /* current NPC dialogue tree    */
    DialogueState  dialogue_state;  /* visual dialogue playback     */
    NPCManager    *npc_manager;     /* NPC system                   */

    /* Inner monologue data loaded from assets/dialogue/monologues.txt */
    MonologueFile  monologue_file;

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
    SDL_Texture *title_screen_texture;

    /* Pause menu */
    Button pause_buttons[2];
    int    pause_choice;

    /* Settings menu */
    float  volume;                     /* 0–100 */
    float  brightness;                 /* 0–100 */
    int    settings_focus;             /* 0=volume, 1=brightness */
    Slider settings_volume_slider;
    Slider settings_brightness_slider;
    Button settings_back_button;

    /* Inventory */
    int selected_inventory_slot;
    SDL_Texture *item_flashlight_texture;  /* icon shown in inventory for flashlight */
    SDL_Texture *item_gasmask_texture;     /* icon shown in inventory for gas mask   */

    /* Flashlight */
    int flashlight_active;    /* 1 if the flashlight beam is on */

    /* Off-screen darkness mask used in the archive room.
     * Filled with opaque black; transparent holes are punched for the
     * ambient circle and the flashlight cone so the room texture shows
     * through those areas when the mask is composited onto the screen. */
    SDL_Texture *darkness_mask;

    /* Item pickup notification */
    char  pickup_item_name[64];  /* name of the last picked-up item */
    float pickup_notify_timer;   /* counts down from > 0; shown while > 0 */

    /* Locker view */
    SDL_Texture *locker_texture;
    SDL_Texture *note_locker_texture;   /* shown when reading the security note */
    int          show_note_locker;      /* 1 = show note_locker_texture instead of locker_texture */
    SDL_Texture *monitor_zoom_texture;  /* shown when examining the security monitor */
    int          show_monitor_zoom;     /* 1 = show monitor_zoom_texture             */

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

/* ── Per-state render helpers ────────────────────────────────────────── */

void game_render_menu(Game *game);
void game_render_playing(Game *game);
void game_render_dialogue_overlay(Game *game);
void game_render_inventory(Game *game);
void game_render_pause(Game *game);
void game_render_settings(Game *game);
void game_render_locker(Game *game);

#endif /* GAME_H */
