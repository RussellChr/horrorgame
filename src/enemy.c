#include "enemy.h"
#include "map.h"
#include "render.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── A* internal limits ─────────────────────────────────────────────────
 * The hallway CSV is 30 rows × 60 cols = 1 800 cells at most.           */
#define ASTAR_MAX 1800

/* ── A* ─────────────────────────────────────────────────────────────────
 *
 * Standard A* on a 4-connected grid.
 *
 * Parameters
 *   grid     – flat [rows*cols] array: 1=walkable, 0=wall
 *   rows/cols – grid dimensions
 *   sr/sc    – start (row, col)
 *   gr/gc    – goal  (row, col)
 *   path     – output array of tile coords (first step first)
 *   max_path – capacity of path[]
 *
 * Returns
 *   > 0 : length of path written into path[]
 *     0 : no path found
 *    -1 : already at goal (no movement needed)
 */
static int astar(const int *grid, int rows, int cols,
                 int sr, int sc, int gr, int gc,
                 TileCoord *path, int max_path)
{
    if (sr == gr && sc == gc) return -1;
    if (gr < 0 || gr >= rows || gc < 0 || gc >= cols) return 0;
    if (!grid[gr * cols + gc]) return 0; /* goal is a wall */

    /* Static working arrays – safe in a single-threaded game. */
    static float g_cost[ASTAR_MAX];
    static int   came_from[ASTAR_MAX]; /* index of parent cell, -1 = none */
    static int   in_closed[ASTAR_MAX];

    typedef struct { int idx; float f; } OpenEntry;
    static OpenEntry open[ASTAR_MAX];
    static int       rev_buf[ASTAR_MAX];

    int total = rows * cols;
    if (total > ASTAR_MAX) total = ASTAR_MAX;

    for (int i = 0; i < total; i++) {
        g_cost[i]    = 1e9f;
        came_from[i] = -1;
        in_closed[i] = 0;
    }

    int open_count = 0;
    int start_idx  = sr * cols + sc;
    int goal_idx   = gr * cols + gc;

    g_cost[start_idx] = 0.0f;
    open[open_count++] = (OpenEntry){
        start_idx,
        (float)(abs(gr - sr) + abs(gc - sc))
    };

    /* 4-directional neighbours */
    const int DR[4] = {-1, 1,  0, 0};
    const int DC[4] = { 0, 0, -1, 1};

    while (open_count > 0) {
        /* Pick node with lowest f (linear scan – fast enough for ≤1 800 nodes) */
        int best = 0;
        for (int i = 1; i < open_count; i++)
            if (open[i].f < open[best].f) best = i;

        int cur_idx = open[best].idx;
        /* Remove from open list */
        open[best] = open[--open_count];

        if (in_closed[cur_idx]) continue;
        in_closed[cur_idx] = 1;

        if (cur_idx == goal_idx) {
            /* Reconstruct path in reverse */
            int rev_len = 0;
            int c = goal_idx;
            while (c != start_idx && rev_len < max_path) {
                rev_buf[rev_len++] = c;
                int p = came_from[c];
                if (p < 0) break;
                c = p;
            }
            /* Write forward-ordered path */
            int n = (rev_len < max_path) ? rev_len : max_path;
            for (int i = 0; i < n; i++) {
                int tidx = rev_buf[n - 1 - i];
                path[i].row = tidx / cols;
                path[i].col = tidx % cols;
            }
            return n;
        }

        int cr = cur_idx / cols;
        int cc = cur_idx % cols;

        for (int d = 0; d < 4; d++) {
            int nr = cr + DR[d];
            int nc = cc + DC[d];
            if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
            int nidx = nr * cols + nc;
            if (!grid[nidx] || in_closed[nidx]) continue;

            float ng = g_cost[cur_idx] + 1.0f;
            if (ng < g_cost[nidx]) {
                g_cost[nidx]    = ng;
                came_from[nidx] = cur_idx;
                float nh = (float)(abs(gr - nr) + abs(gc - nc));
                if (open_count < ASTAR_MAX)
                    open[open_count++] = (OpenEntry){ nidx, ng + nh };
            }
        }
    }
    return 0; /* no path */
}

/* ── Coordinate helpers ─────────────────────────────────────────────── */

static void world_to_tile(const Enemy *e, float wx, float wy,
                           int *row, int *col)
{
    int c = (int)(wx / e->tile_w);
    int r = (int)(wy / e->tile_h);
    if (c < 0) c = 0;
    if (c >= e->grid_cols) c = e->grid_cols - 1;
    if (r < 0) r = 0;
    if (r >= e->grid_rows) r = e->grid_rows - 1;
    *col = c;
    *row = r;
}

static void tile_to_world(const Enemy *e, int row, int col,
                           float *wx, float *wy)
{
    *wx = (col + 0.5f) * e->tile_w;
    *wy = (row + 0.5f) * e->tile_h;
}

/* ── Lifecycle ──────────────────────────────────────────────────────── */

Enemy *enemy_create(const char *hallway_csv,
                    float room_width, float room_height)
{
    Enemy *e = calloc(1, sizeof(Enemy));
    if (!e) return NULL;

    /* AI state */
    e->state  = ENEMY_STATE_PATROL;
    e->wp_dir = 1;
    e->wp_idx = 1; /* head toward waypoint 1 first */

    /* Patrol waypoints (world-space coordinates supplied by the designer) */
    e->wp_x[0] = 1734.0f;  e->wp_y[0] = 319.0f;
    e->wp_x[1] = 1731.0f;  e->wp_y[1] = 583.0f;
    e->wp_x[2] =  710.0f;  e->wp_y[2] = 579.0f;
    e->wp_x[3] =  712.0f;  e->wp_y[3] = 735.0f;
    e->wp_x[4] =  321.0f;  e->wp_y[4] = 733.0f;
    e->wp_x[5] =  321.0f;  e->wp_y[5] = 427.0f;

    /* Spawn at waypoint 0 */
    e->x = e->wp_x[0];
    e->y = e->wp_y[0];

    /* Build walkability grid from the hallway CSV */
    Map *m = map_load_csv(hallway_csv);
    if (m) {
        int total = m->rows * m->cols;
        e->grid = malloc((size_t)total * sizeof(int));
        if (e->grid) {
            for (int i = 0; i < total; i++)
                /* MAP_TILE_WALL = 0; every other value is walkable */
                e->grid[i] = (m->cells[i] != 0) ? 1 : 0;
        }
        e->grid_rows = m->rows;
        e->grid_cols = m->cols;
        map_free(m);
    }

    if (e->grid_cols > 0 && e->grid_rows > 0 &&
        room_width > 0.0f   && room_height > 0.0f) {
        e->tile_w = room_width  / (float)e->grid_cols;
        e->tile_h = room_height / (float)e->grid_rows;
    }

    return e;
}

void enemy_destroy(Enemy *e)
{
    if (!e) return;
    free(e->grid);
    free(e);
}

/* ── Movement helper ────────────────────────────────────────────────── */

/* Move the enemy centre toward (tx, ty) at 'speed' px/s.
 * Returns 1 when the target has been reached (within 4 px). */
static int move_toward(Enemy *e, float tx, float ty,
                        float speed, float dt)
{
    float dx   = tx - e->x;
    float dy   = ty - e->y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 4.0f) return 1;

    float step = speed * dt;
    if (step >= dist) {
        e->x = tx;
        e->y = ty;
        return 1;
    }
    e->x += (dx / dist) * step;
    e->y += (dy / dist) * step;
    return 0;
}

/* ── Update ─────────────────────────────────────────────────────────── */

int enemy_update(Enemy *e, float player_x, float player_y, float dt)
{
    if (!e) return 0;

    /* Distance from enemy centre to player position */
    float pdx   = player_x - e->x;
    float pdy   = player_y - e->y;
    float pdist = sqrtf(pdx * pdx + pdy * pdy);

    /* ── State transitions ── */
    if (e->state == ENEMY_STATE_PATROL &&
        pdist <= ENEMY_DETECT_RADIUS) {
        e->state      = ENEMY_STATE_CHASE;
        e->path_len   = 0;
        e->path_step  = 0;
        e->path_timer = 0.0f; /* force immediate path computation */
    } else if (e->state == ENEMY_STATE_CHASE &&
               pdist > ENEMY_LOSE_RADIUS) {
        e->state = ENEMY_STATE_PATROL;
    }

    /* ── Game-over check ── */
    if (pdist <= ENEMY_CATCH_DIST) return 1;

    /* ── Patrol: move toward the current waypoint ── */
    if (e->state == ENEMY_STATE_PATROL) {
        float tx = e->wp_x[e->wp_idx];
        float ty = e->wp_y[e->wp_idx];

        if (move_toward(e, tx, ty, ENEMY_PATROL_SPEED, dt)) {
            /* Advance waypoint index with ping-pong */
            e->wp_idx += e->wp_dir;
            if (e->wp_idx >= ENEMY_NUM_WAYPOINTS) {
                e->wp_idx = ENEMY_NUM_WAYPOINTS - 2;
                e->wp_dir = -1;
            } else if (e->wp_idx < 0) {
                e->wp_idx = 1;
                e->wp_dir = 1;
            }
        }

    /* ── Chase: follow A* path toward the player ── */
    } else {
        e->path_timer -= dt;

        /* Recompute path when timer expires or path is exhausted */
        if (e->path_timer <= 0.0f || e->path_step >= e->path_len) {
            e->path_timer = ENEMY_PATH_RETHINK;

            if (e->grid && e->tile_w > 0.0f && e->tile_h > 0.0f) {
                int er, ec, pr, pc;
                world_to_tile(e, e->x,      e->y,      &er, &ec);
                world_to_tile(e, player_x,  player_y,  &pr, &pc);
                int len = astar(e->grid, e->grid_rows, e->grid_cols,
                                er, ec, pr, pc,
                                e->path, ENEMY_MAX_PATH);
                e->path_len  = (len > 0) ? len : 0;
                e->path_step = 0;
            }
        }

        if (e->path_step < e->path_len) {
            /* Follow the next tile in the computed path */
            float tx, ty;
            tile_to_world(e,
                          e->path[e->path_step].row,
                          e->path[e->path_step].col,
                          &tx, &ty);
            if (move_toward(e, tx, ty, ENEMY_CHASE_SPEED, dt))
                e->path_step++;
        } else {
            /* No valid path – head directly toward the player */
            move_toward(e, player_x, player_y, ENEMY_CHASE_SPEED, dt);
        }
    }

    return 0;
}

/* ── Render ─────────────────────────────────────────────────────────── */

void enemy_render(Enemy *e, SDL_Renderer *renderer,
                  int screen_x, int screen_y)
{
    if (!e || !renderer) return;

    int x = screen_x - ENEMY_W / 2;
    int y = screen_y - ENEMY_H / 2;

    /* Dark-red fill */
    render_filled_rect(renderer, x, y, ENEMY_W, ENEMY_H,
                       180, 20, 20, 230);
    /* Bright-red outline */
    render_rect_outline(renderer, x, y, ENEMY_W, ENEMY_H,
                        255, 60, 60, 255);
}
