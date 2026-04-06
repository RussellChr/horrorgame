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

/* ── Monitor passcode constants ────────────────────────────────────────── */
#define MONITOR_PANEL_X      685
#define MONITOR_PANEL_Y      415
#define MONITOR_PANEL_W       75
#define MONITOR_PANEL_H      119
#define PASSCODE_CORRECT     "3643"
/* Display buffer: 4 digits alternating with spaces + null  ("_ _ _ _\0") */
#define PASSCODE_DISPLAY_SIZE  8

/* ── AM recording interactable (monitor_zoom screen) ──────────────────── */
#define AM_RECORD_X   377
#define AM_RECORD_Y   274
#define AM_RECORD_W    66   /* 443 - 377 */
#define AM_RECORD_H   109   /* 383 - 274 */

/* ── Game states ──────────────────────────────────────────────────────── */

typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_PLAYING,
    GAME_STATE_DIALOGUE,
    GAME_STATE_INVENTORY,
    GAME_STATE_PAUSE,
    GAME_STATE_SETTINGS,
    GAME_STATE_LOCKER,
    GAME_STATE_SIMON,
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
    SDL_Texture *item_keycard_texture;     /* icon shown in inventory for keycard    */

    /* Flashlight */
    int flashlight_active;    /* 1 if the flashlight beam is on */

    /* Gas mask */
    int gasmask_active;       /* 1 if the gas mask vignette is on */

    /* Lab poisonous gas */
    float lab_gas_timer;      /* counts down from LAB_GAS_DEATH_DELAY when in lab without mask */
    int   lab_death_triggered; /* 1 after the gas-death dialogue has been queued */

    /* Archive room darkness: full-screen light-mask render target */
    SDL_Texture *dark_overlay;

    /* Item pickup notification */
    char  pickup_item_name[64];  /* name of the last picked-up item */
    float pickup_notify_timer;   /* counts down from > 0; shown while > 0 */

    /* Locker view */
    SDL_Texture *locker_texture;
    SDL_Texture *note_locker_texture;   /* shown when reading the security note */
    int          show_note_locker;      /* 1 = show note_locker_texture instead of locker_texture */
    SDL_Texture *monitor_zoom_texture;  /* shown when examining the security monitor */
    int          show_monitor_zoom;     /* 1 = show monitor_zoom_texture             */

    /* Passcode system (triggered by clicking the monitor panel rect) */
    int  passcode_active;          /* 1 = passcode input overlay is shown */
    char passcode_input[5];        /* null-terminated 4-digit input buffer */
    int  passcode_input_len;       /* number of digits entered so far */
    int  passcode_wrong;           /* 1 = wrong code was submitted, show error */
    int  passcode_correct;         /* 1 = correct code just entered, show success */

    /* AM recording audio (played when the AM recording interactable is clicked) */
    SDL_AudioSpec    am_wav_spec;
    Uint8           *am_wav_buf;
    Uint32           am_wav_len;
    SDL_AudioStream *am_audio_stream;

    /* Timing */
    Uint64 last_ticks;
    float  delta_time;

    /* Keyboard state (pointer into SDL's internal array) */
    const bool *keys;

    /* Running flag */
    int running;

    /* Simon Says minigame */
    int   simon_sequence[10];    /* button indices: 0=Red,1=Blue,2=Green,3=Yellow */
    int   simon_length;          /* steps in the current sequence (1–10)          */
    int   simon_show_pos;        /* which step is being shown during show phase    */
    int   simon_show_lit;        /* 1=button currently lit, 0=dark gap             */
    float simon_show_timer;      /* countdown for current show-phase step          */
    int   simon_player_pos;      /* player's progress in matching the sequence     */
    int   simon_phase;           /* 0=showing, 1=player-input, 2=round-pause       */
    int   simon_lit_button;      /* which button is lit right now (-1 = none)      */
    int   simon_death_triggered; /* 1 if player failed the Simon game              */
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
void game_start_simon(Game *game);

/* ── Per-state render helpers ────────────────────────────────────────── */

void game_render_menu(Game *game);
void game_render_playing(Game *game);
void game_render_dialogue_overlay(Game *game);
void game_render_inventory(Game *game);
void game_render_pause(Game *game);
void game_render_settings(Game *game);
void game_render_locker(Game *game);
void game_render_simon(Game *game);

#endif /* GAME_H */
