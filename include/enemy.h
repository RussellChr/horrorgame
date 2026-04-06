#ifndef ENEMY_INCLUDE_H
#define ENEMY_INCLUDE_H

#include <SDL3/SDL.h>
#include "camera.h"
#include "animation.h"

/* ── Enemy constants ──────────────────────────────────────────────────── */

#define ENEMY_WAYPOINT_COUNT      6
#define ENEMY_PATH_MAX          256
#define ENEMY_MAX_ANIM_FRAMES     8
#define ENEMY_BACKWARD_FRAMES     6  /* available backward asset frames */
#define ENEMY_RIGHT_FRAMES        6  /* available right asset frames */
#define ENEMY_W                 116    /* ~1.5x original 77px sprite width  */
#define ENEMY_H                 116    /* ~1.5x original 77px sprite height */

#define ENEMY_PATROL_SPEED      100.0f /* pixels/second while patrolling     */
#define ENEMY_CHASE_SPEED       200.0f /* pixels/second while chasing        */
#define ENEMY_CHASE_RADIUS      350.0f /* distance at which chase begins     */
#define ENEMY_PATROL_RADIUS     500.0f /* distance at which chase ends       */
#define ENEMY_HIT_DIST           36.0f /* distance that triggers game over   */
#define ENEMY_WAYPOINT_THRESH    18.0f /* "reached" threshold for a waypoint */
#define ENEMY_REPATH_INTERVAL     0.4f /* seconds between A* recalculations  */

/* ── Enemy states ─────────────────────────────────────────────────────── */

typedef enum {
    ENEMY_STATE_INACTIVE = 0,
    ENEMY_STATE_PATROL,
    ENEMY_STATE_CHASE
} EnemyState;

typedef enum {
    ENEMY_DIR_FORWARD = 0,
    ENEMY_DIR_BACKWARD,
    ENEMY_DIR_LEFT,
    ENEMY_DIR_RIGHT
} EnemyDirection;

/* ── Simple 2D world-space point ──────────────────────────────────────── */

typedef struct {
    float x, y;
} EnemyPoint;

/* ── Enemy ────────────────────────────────────────────────────────────── */

typedef struct {
    /* World-space position (top-left of the enemy rect) */
    float x, y;

    /* State machine */
    int   state;   /* EnemyState */
    int   active;  /* 1 = spawned and updating */

    /* Patrol */
    EnemyPoint waypoints[ENEMY_WAYPOINT_COUNT];
    int        waypoint_count;
    int        current_waypoint; /* index of the waypoint we are heading to */
    int        patrol_dir;       /* +1 = forward (0→5), -1 = backward (5→0) */

    /* A* chase path (world-space node centres) */
    EnemyPoint path[ENEMY_PATH_MAX];
    int        path_len;
    int        path_idx;       /* index of the next node to move towards */
    float      repath_timer;   /* countdown to the next A* recalculation */

    /* Hallway tile grid used for A* (owned by this struct) */
    int  *grid;
    int   grid_rows;
    int   grid_cols;
    float tile_w;   /* world pixels per grid column */
    float tile_h;   /* world pixels per grid row    */

    /* Visuals */
    Animation     move_anim; /* movement animation state (5 FPS looping) */
    EnemyDirection direction;
    EnemyDirection last_anim_direction; /* previous direction used for move_anim frame */
    int           is_moving;
    int           saved_frame_by_dir[4]; /* cached frame index per EnemyDirection */
    SDL_Texture  *forward_frames[ENEMY_MAX_ANIM_FRAMES];
    int           forward_count;
    SDL_Texture  *backward_frames[ENEMY_BACKWARD_FRAMES];
    int           backward_count;
    SDL_Texture  *left_frames[ENEMY_MAX_ANIM_FRAMES];
    int           left_count;
    SDL_Texture  *right_frames[ENEMY_RIGHT_FRAMES];
    int           right_count;
} Enemy;

/* ── API ──────────────────────────────────────────────────────────────── */

/*
 * Initialise the enemy: load the hallway CSV, store the grid for A*, and
 * set the six patrol waypoints.  Must be called after world_setup_rooms() so
 * room_width / room_height are known.
 */
void enemy_init(Enemy *e, int room_width, int room_height);
void enemy_load_sprites(Enemy *e, SDL_Renderer *renderer);

/* Free the grid memory allocated by enemy_init(). */
void enemy_free(Enemy *e);

/*
 * Per-frame update.
 *   player_x / player_y : player world-space position.
 *   player_in_room      : 1 if the player is currently in the hallway.
 *   dt                  : elapsed seconds since the last frame.
 */
void enemy_update(Enemy *e, float player_x, float player_y,
                  int player_in_room, float dt);

/* Returns 1 if the enemy has caught the player (game-over condition). */
int  enemy_hits_player(const Enemy *e, float player_x, float player_y);

/* Render the enemy rect.  cam is used to convert world→screen coordinates. */
void enemy_render(const Enemy *e, SDL_Renderer *renderer, const Camera *cam);

#endif /* ENEMY_INCLUDE_H */
