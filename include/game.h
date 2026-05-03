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
#include "video.h"
#include "enemy.h"
#include "savegame.h"

/* ── Monitor passcode constants ────────────────────────────────────────── */
#define MONITOR_PANEL_X      685
#define MONITOR_PANEL_Y      415
#define MONITOR_PANEL_W       75
#define MONITOR_PANEL_H      119
#define PASSCODE_CORRECT     "3643"
/* Display buffer: 4 digits alternating with spaces + null  ("_ _ _ _\0") */
#define PASSCODE_DISPLAY_SIZE  8

/* ── Archive room glass shard constants ─────────────────────────────────── */
#define ARCHIVE_GLASS_COUNT  6
#define ARCHIVE_GLASS_SIZE  40
extern const SDL_Point archive_glass_positions[ARCHIVE_GLASS_COUNT];

/* ── AM recording interactable (monitor_zoom screen) ──────────────────── */
#define AM_RECORD_X   377
#define AM_RECORD_Y   274
#define AM_RECORD_W    66   /* 443 - 377 */
#define AM_RECORD_H   109   /* 383 - 274 */

/* ── Containment level interactable (monitor_zoom screen) ─────────────── */
#define CONTAINMENT_LEVEL_RECT_X   990
#define CONTAINMENT_LEVEL_RECT_Y   168
#define CONTAINMENT_LEVEL_RECT_W   125
#define CONTAINMENT_LEVEL_RECT_H   122

/* ── Game states ──────────────────────────────────────────────────────── */

typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_PLAYING,
    GAME_STATE_DIALOGUE,
    GAME_STATE_INVENTORY,
    GAME_STATE_PAUSE,
    GAME_STATE_SETTINGS,
    GAME_STATE_LOCKER,
    GAME_STATE_CUTSCENE,
    GAME_STATE_SIMON,
    GAME_STATE_JUMPSCARE,
    GAME_STATE_GAME_OVER,
    GAME_STATE_NEW_LOAD_MENU,   /* title-screen "New / Load Game" submenu  */
    GAME_STATE_SAVE_MENU,       /* in-game save-slot selection             */
    GAME_STATE_LOAD_MENU,       /* save-slot selection for loading         */
    GAME_STATE_ARCHIVE_BOOK,    /* reading archive pages (pg1 / pg2)       */
    GAME_STATE_QUIT
} GameState;

/* ── Cutscene type identifiers ────────────────────────────────────────── */
#define CUTSCENE_TYPE_HIBERNATION  0   /* intro cutscene shown at game start */
#define CUTSCENE_TYPE_SECURITY     1   /* cutscene after correct passcode    */
#define CUTSCENE_TYPE_POWER        2   /* cutscene after Simon Says win      */

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

    /* New / Load submenu (title screen) */
    Button new_load_buttons[3];        /* New Game, Load Game, Back */
    int    new_load_menu_choice;

    /* Pause menu */
    Button pause_buttons[4];           /* Resume, Save, Load, Quit to Menu */
    int    pause_choice;

    /* Save / Load slot selection menus */
    Button    save_load_buttons[4];              /* Slot 1–3 + Cancel         */
    char      save_slot_labels[SAVE_SLOT_COUNT][64]; /* dynamic slot text     */
    GameState save_load_prev_state;              /* state to return to on Cancel */

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
    SDL_Texture *item_fingerprint_texture; /* icon shown in inventory for fingerprint */
    SDL_Texture *item_thermalfuse_texture; /* icon shown in inventory for thermal fuse */

    /* Flashlight */
    int flashlight_active;    /* 1 if the flashlight beam is on */

    /* Gas mask */
    int gasmask_active;       /* 1 if the gas mask vignette is on */

    /* Lab poisonous gas */
    float lab_gas_timer;      /* counts down from LAB_GAS_DEATH_DELAY when in lab without mask */
    int   lab_death_triggered; /* 1 after the gas-death dialogue has been queued */

    /* Archive room darkness: full-screen light-mask render target */
    SDL_Texture *dark_overlay;
    float ambient_flicker_timer;      /* seconds until next rare flicker pulse */
    float ambient_flicker_duration;   /* remaining seconds of active flicker pulse */
    Uint8 ambient_flicker_alpha;      /* extra darkness alpha applied during pulse */

    /* Archive room glass overlay */
    SDL_Texture *glass_texture;
    int          archive_glass_collected[6]; /* 1 = this shard was stepped on */

    /* Item pickup notification */
    char  pickup_item_name[64];  /* name of the last picked-up item */
    float pickup_notify_timer;   /* counts down from > 0; shown while > 0 */

    /* Locker view */
    SDL_Texture *locker_texture;
    SDL_Texture *note_locker_texture;   /* shown when reading the security note */
    int          show_note_locker;      /* 1 = show note_locker_texture instead of locker_texture */
    SDL_Texture *monitor_zoom_texture;        /* shown when examining the security monitor  */
    int          show_monitor_zoom;           /* 1 = show monitor_zoom_texture              */
    SDL_Texture *containment_level_texture;   /* shown when clicking the containment rect   */
    int          show_containment_level;      /* 1 = show containment_level_texture overlay */

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

    /* Glass cracking SFX (played when player steps on an archive glass shard) */
    SDL_AudioSpec    glass_crack_wav_spec;
    Uint8           *glass_crack_wav_buf;
    Uint32           glass_crack_wav_len;
    SDL_AudioStream *glass_crack_audio_stream;

    /* Enemy patrol / chase system */
    Enemy         enemy;          /* hallway enemy (activated by passcode)    */
    Enemy         archive_enemy;  /* archive enemy (activated on first visit) */

    /* Timing */
    Uint64 last_ticks;
    float  delta_time;

    /* Keyboard state (pointer into SDL's internal array) */
    const bool *keys;

    /* Running flag */
    int running;

    /* Cutscene system (shared by all cutscene types) */
    int           cutscene_type;                 /* 0=hibernation, 1=security, 2=power */
    int           cutscene_index;                /* current scene index                */
    DialogueState cutscene_dialogue_state;       /* typewriter/render                  */
    DialogueTree *cutscene_dialogue_tree;        /* text for the current scene         */

    /* Hibernation cutscene (shown on new game) */
    SDL_Texture  *hibernation_cutscene_textures[3]; /* scene images 1–3 */

    /* Security cutscene (shown once after correct passcode) */
    SDL_Texture  *security_cutscene_textures[4]; /* scene images 1–4 */
    int           security_cutscene_played;      /* 1 once shown     */

    /* Power cutscene (shown once after Simon Says is won) */
    SDL_Texture  *power_cutscene_textures[3];    /* scene images 1–3 */

    /* Archive book viewer (triggered by tile-6 in archive_close.csv) */
    SDL_Texture *archive_pg1_texture;       /* first page image              */
    SDL_Texture *archive_pg2_texture;       /* second page image             */
    int          archive_book_page;         /* 0 = pg1, 1 = pg2              */
    float        archive_book_trans_timer;  /* >0 while black-screen fading  */
    int          archive_book_next_page;    /* page to show after transition */

    /* Simon Says minigame */
    int   simon_sequence[8];     /* button indices: 0=Red,1=Blue,2=Green,3=Yellow */
    int   simon_length;          /* steps in the current sequence (1–8)           */
    int   simon_show_pos;        /* which step is being shown during show phase    */
    int   simon_show_lit;        /* 1=button currently lit, 0=dark gap             */
    float simon_show_timer;      /* countdown for current show-phase step          */
    int   simon_player_pos;      /* player's progress in matching the sequence     */
    int   simon_phase;           /* 0=showing, 1=player-input, 2=round-pause       */
    int   simon_lit_button;      /* which button is lit right now (-1 = none)      */
    int   simon_death_triggered;   /* 1 if player failed the Simon game            */
    int   simon_jumpscare_played;  /* 1 after the jumpscare has been shown once    */

    /* Jumpscare (shown 1 s after round-7 pattern display finishes) */
    VideoPlayer *jumpscare_player;  /* active during GAME_STATE_JUMPSCARE    */
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
void game_start_hibernation_cutscene(Game *game);
void game_start_security_cutscene(Game *game);
void game_start_power_cutscene(Game *game);

/* ── Per-state render helpers ────────────────────────────────────────── */

void game_render_menu(Game *game);
void game_render_playing(Game *game);
void game_render_dialogue_overlay(Game *game);
void game_render_inventory(Game *game);
void game_render_pause(Game *game);
void game_render_settings(Game *game);
void game_render_locker(Game *game);
void game_render_cutscene(Game *game);
void game_render_simon(Game *game);
void game_render_jumpscare(Game *game);
void game_render_game_over(Game *game);
void game_render_archive_book(Game *game);
void game_render_new_load_menu(Game *game);
void game_render_save_menu(Game *game);
void game_render_load_menu(Game *game);

#endif /* GAME_H */
