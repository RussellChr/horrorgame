#ifndef GAME_H
#define GAME_H

#include "player.h"
#include "world.h"
#include "story.h"
#include "dialogue.h"

/* ── Item ID constants ─────────────────────────────────────────────────── */

#define ITEM_ID_CANDLE        0
#define ITEM_ID_DIARY         1
#define ITEM_ID_BASEMENT_KEY  10

/* ── Location ID constants ─────────────────────────────────────────────── */

#define LOC_ID_ENTRANCE_HALL  0
#define LOC_ID_CORRIDOR       1
#define LOC_ID_LIBRARY        2
#define LOC_ID_BASEMENT       3
#define LOC_ID_CHILDS_ROOM    4
#define LOC_ID_RITUAL_ROOM    5

/* ── Game status codes ─────────────────────────────────────────────────── */

typedef enum {
    GAME_STATUS_RUNNING = 0,
    GAME_STATUS_PAUSED,
    GAME_STATUS_WIN,
    GAME_STATUS_LOSE,
    GAME_STATUS_QUIT
} GameStatus;

/* ── Game configuration ────────────────────────────────────────────────── */

typedef struct {
    int  slow_text;         /* 1 = typewriter effect, 0 = instant        */
    int  text_delay_ms;     /* milliseconds per character                 */
    int  ansi_colour;       /* 1 = use ANSI colour codes                  */
    char save_path[256];    /* path to save-file directory                */
    char asset_path[256];   /* path to assets directory                   */
} GameConfig;

/* ── GameState ─────────────────────────────────────────────────────────── */

typedef struct {
    GameConfig    config;
    GameStatus    status;
    Player       *player;
    World        *world;
    StoryState   *story;
    DialogueTree *dialogue;
    int           turn_count;
} GameState;

/* ── API ───────────────────────────────────────────────────────────────── */

/* Lifecycle */
GameState *game_init(const char *player_name, const GameConfig *cfg);
void       game_run(GameState *gs);
void       game_shutdown(GameState *gs);

/* Turn processing */
void game_process_command(GameState *gs, const char *cmd);
void game_handle_movement(GameState *gs, const char *direction);
void game_handle_look(GameState *gs);
void game_handle_take(GameState *gs, const char *item_name);
void game_handle_use(GameState *gs, const char *item_name);
void game_handle_talk(GameState *gs);
void game_handle_inventory(GameState *gs);
void game_handle_status(GameState *gs);
void game_handle_help(void);

/* Checks run each turn */
void game_check_win_lose(GameState *gs);

/* Save / load */
int  game_save(const GameState *gs, const char *filepath);
int  game_load(GameState *gs, const char *filepath);

#endif /* GAME_H */
