#ifndef ENEMY_H
#define ENEMY_H

#include <SDL3/SDL.h>

/* ── Enemy dimensions (world-space pixels) ────────────────────────────── */
#define ENEMY_W  20
#define ENEMY_H  20

/* ── Speed (world pixels per second) ─────────────────────────────────── */
#define ENEMY_PATROL_SPEED   90.0f
#define ENEMY_CHASE_SPEED   210.0f

/* ── Detection radii ─────────────────────────────────────────────────── */
#define ENEMY_DETECT_RADIUS  300.0f   /* player within this → switch to chase */
#define ENEMY_LOSE_RADIUS    450.0f   /* player beyond this → return to patrol */
#define ENEMY_CATCH_DIST      40.0f   /* enemy this close → game over          */

/* ── A* tuning ───────────────────────────────────────────────────────── */
#define ENEMY_PATH_RETHINK   0.30f    /* seconds between path recalculations   */
#define ENEMY_MAX_PATH       512      /* max tiles in a single A* path         */

/* ── Waypoint count ──────────────────────────────────────────────────── */
#define ENEMY_NUM_WAYPOINTS  6

/* ── Enemy AI states ─────────────────────────────────────────────────── */

typedef enum {
    ENEMY_STATE_PATROL,
    ENEMY_STATE_CHASE
} EnemyState;

/* ── Tile coordinate (used for A* path storage) ──────────────────────── */

typedef struct {
    int row, col;
} TileCoord;

/* ── Enemy ───────────────────────────────────────────────────────────── */

typedef struct {
    /* World-space centre position */
    float x, y;

    /* Current AI state */
    EnemyState state;

    /* Patrol waypoints – ping-pong through indices 0 … ENEMY_NUM_WAYPOINTS-1 */
    float wp_x[ENEMY_NUM_WAYPOINTS];
    float wp_y[ENEMY_NUM_WAYPOINTS];
    int   wp_idx;   /* index of the waypoint currently targeted */
    int   wp_dir;   /* +1 = forward, -1 = backward              */

    /* A* chase path */
    TileCoord path[ENEMY_MAX_PATH];
    int       path_len;    /* number of valid steps in path[]   */
    int       path_step;   /* index of the next step to follow  */
    float     path_timer;  /* countdown to next recompute (s)   */

    /* Navigation grid built from the hallway CSV
     * flat array [row * cols + col]: 1 = walkable, 0 = wall */
    int  *grid;
    int   grid_rows;
    int   grid_cols;
    float tile_w;   /* world pixels per tile column */
    float tile_h;   /* world pixels per tile row    */
} Enemy;

/* ── API ─────────────────────────────────────────────────────────────── */

/*
 * Create an enemy for the hallway.
 * Loads the walkability grid from 'hallway_csv'.
 * room_width / room_height are the world-space dimensions of the room.
 */
Enemy *enemy_create(const char *hallway_csv,
                    float room_width, float room_height);
void   enemy_destroy(Enemy *enemy);

/*
 * Per-frame update.
 * player_x / player_y are the player's world-space position.
 * Returns 1 if the enemy has caught the player (game over), 0 otherwise.
 */
int  enemy_update(Enemy *enemy, float player_x, float player_y, float dt);

/*
 * Render the enemy.
 * screen_x / screen_y are the screen-space coordinates of the enemy centre
 * (the caller performs the camera transform before calling this).
 */
void enemy_render(Enemy *enemy, SDL_Renderer *renderer,
                  int screen_x, int screen_y);

#endif /* ENEMY_H */
