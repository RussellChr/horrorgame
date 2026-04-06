#include "enemy.h"
#include "map.h"
#include "render.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdio.h>

/* ── A* pathfinding ───────────────────────────────────────────────────── */

#define ASTAR_GRID_MAX (30 * 60)   /* upper bound on grid cells */

/* Snap a grid coordinate to the nearest walkable cell by expanding outward. */
static void snap_to_walkable(const int *grid, int rows, int cols,
                              int *row, int *col)
{
    if (grid[(*row) * cols + (*col)] != 0)
        return; /* already walkable */

    for (int d = 1; d < rows + cols; d++) {
        for (int dr = -d; dr <= d; dr++) {
            for (int dc = -d; dc <= d; dc++) {
                if (abs(dr) + abs(dc) != d) continue; /* Manhattan shell */
                int tr = *row + dr, tc = *col + dc;
                if (tr < 0 || tr >= rows || tc < 0 || tc >= cols) continue;
                if (grid[tr * cols + tc] != 0) {
                    *row = tr; *col = tc;
                    return;
                }
            }
        }
    }
}

/*
 * Run A* on the grid from (sr,sc) to (gr,gc).
 * Fills path_out[] with world-space centres of each node (excluding start).
 * Returns the path length (0 = no path found or already at goal).
 */
static int astar(const int *grid, int rows, int cols,
                 int sr, int sc, int gr, int gc,
                 EnemyPoint *path_out, int *path_len_out,
                 float tile_w, float tile_h)
{
    /* Clamp inputs */
    if (sr < 0) sr = 0; if (sr >= rows) sr = rows - 1;
    if (sc < 0) sc = 0; if (sc >= cols) sc = cols - 1;
    if (gr < 0) gr = 0; if (gr >= rows) gr = rows - 1;
    if (gc < 0) gc = 0; if (gc >= cols) gc = cols - 1;

    /* Snap start and goal to walkable cells */
    snap_to_walkable(grid, rows, cols, &sr, &sc);
    snap_to_walkable(grid, rows, cols, &gr, &gc);

    int n     = rows * cols;
    int start = sr * cols + sc;
    int goal  = gr * cols + gc;

    if (start == goal) {
        *path_len_out = 0;
        return 1;
    }

    float *g_cost    = malloc(sizeof(float) * (size_t)n);
    float *f_cost    = malloc(sizeof(float) * (size_t)n);
    int   *came_from = malloc(sizeof(int)   * (size_t)n);
    char  *closed    = calloc((size_t)n, 1);
    char  *in_open   = calloc((size_t)n, 1);

    if (!g_cost || !f_cost || !came_from || !closed || !in_open) {
        free(g_cost); free(f_cost); free(came_from);
        free(closed); free(in_open);
        *path_len_out = 0;
        return 0;
    }

    for (int i = 0; i < n; i++) {
        g_cost[i]    = FLT_MAX;
        f_cost[i]    = FLT_MAX;
        came_from[i] = -1;
    }
    g_cost[start]  = 0.0f;
    f_cost[start]  = fabsf((float)(sr - gr)) + fabsf((float)(sc - gc));
    in_open[start] = 1;

    static const int drow[8] = {-1,  1,  0,  0, -1, -1,  1,  1};
    static const int dcol[8] = { 0,  0, -1,  1, -1,  1, -1,  1};
    static const float step_cost[8] = {1,1,1,1, 1.414f,1.414f,1.414f,1.414f};

    int found = 0;
    while (1) {
        /* Find lowest-f node in the open set (simple linear scan — fast
         * enough for a 30×60 = 1800-cell grid). */
        int   cur  = -1;
        float best = FLT_MAX;
        for (int i = 0; i < n; i++) {
            if (in_open[i] && f_cost[i] < best) {
                best = f_cost[i];
                cur  = i;
            }
        }
        if (cur < 0) break; /* exhausted — no path */
        if (cur == goal) { found = 1; break; }

        in_open[cur] = 0;
        closed[cur]  = 1;

        int cr = cur / cols, cc = cur % cols;
        for (int d = 0; d < 8; d++) {
            int nr = cr + drow[d], nc = cc + dcol[d];
            if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
            int nb = nr * cols + nc;
            if (closed[nb])       continue;
            if (grid[nb] == 0)    continue; /* wall */

            float ng = g_cost[cur] + step_cost[d];
            if (ng < g_cost[nb]) {
                came_from[nb] = cur;
                g_cost[nb]    = ng;
                float h       = fabsf((float)(nr - gr)) + fabsf((float)(nc - gc));
                f_cost[nb]    = ng + h;
                in_open[nb]   = 1;
            }
        }
    }

    int path_len = 0;
    if (found) {
        /* Reconstruct the path (reversed) into a temporary buffer */
        int tmp[ASTAR_GRID_MAX];
        int tmp_len = 0;
        int node = goal;
        while (node != start && node >= 0 && tmp_len < ASTAR_GRID_MAX) {
            tmp[tmp_len++] = node;
            node = came_from[node];
        }
        /* Reverse into path_out (capped at ENEMY_PATH_MAX) */
        path_len = (tmp_len < ENEMY_PATH_MAX) ? tmp_len : ENEMY_PATH_MAX;
        for (int i = 0; i < path_len; i++) {
            int idx = tmp[path_len - 1 - i];
            int pr  = idx / cols;
            int pc  = idx % cols;
            path_out[i].x = (float)pc * tile_w + tile_w * 0.5f;
            path_out[i].y = (float)pr * tile_h + tile_h * 0.5f;
        }
    }
    *path_len_out = path_len;

    free(g_cost); free(f_cost); free(came_from);
    free(closed); free(in_open);
    return found;
}

/* ── Init / Free ──────────────────────────────────────────────────────── */

void enemy_init(Enemy *e, int room_width, int room_height)
{
    if (!e) return;
    memset(e, 0, sizeof(*e));

    /* Load the hallway tile grid for A* */
    Map *m = map_load_csv("maps/hallway.csv");
    if (m) {
        int n = m->rows * m->cols;
        e->grid = malloc(sizeof(int) * (size_t)n);
        if (e->grid)
            memcpy(e->grid, m->cells, sizeof(int) * (size_t)n);
        e->grid_rows = m->rows;
        e->grid_cols = m->cols;
        map_free(m);
    } else {
        /* Fallback: 1×1 grid with a single walkable cell so the rest of the
         * code is safe.  -1 is MAP_TILE_FLOOR; A* treats any non-zero value
         * (including -1) as walkable. */
        e->grid      = calloc(1, sizeof(int));
        if (e->grid) e->grid[0] = -1; /* MAP_TILE_FLOOR = walkable */
        e->grid_rows = 1;
        e->grid_cols = 1;
    }

    /* Compute tile dimensions from room size and grid dimensions */
    e->tile_w = (e->grid_cols > 0) ? (float)room_width  / (float)e->grid_cols : 32.0f;
    e->tile_h = (e->grid_rows > 0) ? (float)room_height / (float)e->grid_rows : 32.0f;

    /* Patrol waypoints (world-space coordinates) */
    e->waypoints[0] = (EnemyPoint){1734.0f, 319.0f};
    e->waypoints[1] = (EnemyPoint){1731.0f, 583.0f};
    e->waypoints[2] = (EnemyPoint){710.0f,  579.0f};
    e->waypoints[3] = (EnemyPoint){712.0f,  735.0f};
    e->waypoints[4] = (EnemyPoint){321.0f,  733.0f};
    e->waypoints[5] = (EnemyPoint){321.0f,  427.0f};
    e->waypoint_count   = ENEMY_WAYPOINT_COUNT;
    e->current_waypoint = 0;
    e->patrol_dir       = 1; /* start moving forward */

    /* Spawn at first waypoint (inactive until activated) */
    e->x = e->waypoints[0].x;
    e->y = e->waypoints[0].y;

    e->state  = ENEMY_STATE_INACTIVE;
    e->active = 0;
    e->direction = ENEMY_DIR_FORWARD;
    e->is_moving = 0;
    animation_init(&e->move_anim, 8, 8.0f, 1);
}

void enemy_free(Enemy *e)
{
    if (!e) return;
    for (int i = 0; i < e->forward_count; i++)
        if (e->forward_frames[i]) render_texture_destroy(e->forward_frames[i]);
    for (int i = 0; i < e->backward_count; i++)
        if (e->backward_frames[i]) render_texture_destroy(e->backward_frames[i]);
    for (int i = 0; i < e->left_count; i++)
        if (e->left_frames[i]) render_texture_destroy(e->left_frames[i]);
    for (int i = 0; i < e->right_count; i++)
        if (e->right_frames[i]) render_texture_destroy(e->right_frames[i]);

    free(e->grid);
    e->grid = NULL;
    e->forward_count = 0;
    e->backward_count = 0;
    e->left_count = 0;
    e->right_count = 0;
}

/* ── Helpers ──────────────────────────────────────────────────────────── */

/* Convert a world-space x to a grid column index (clamped). */
static int world_to_col(const Enemy *e, float wx)
{
    int c = (int)(wx / e->tile_w);
    if (c < 0) c = 0;
    if (c >= e->grid_cols) c = e->grid_cols - 1;
    return c;
}

/* Convert a world-space y to a grid row index (clamped). */
static int world_to_row(const Enemy *e, float wy)
{
    int r = (int)(wy / e->tile_h);
    if (r < 0) r = 0;
    if (r >= e->grid_rows) r = e->grid_rows - 1;
    return r;
}

/* Centre of the enemy rect in world space. */
static void enemy_centre(const Enemy *e, float *cx, float *cy)
{
    *cx = e->x + ENEMY_W * 0.5f;
    *cy = e->y + ENEMY_H * 0.5f;
}

/* Euclidean distance from the enemy centre to a world-space point. */
static float enemy_dist(const Enemy *e, float wx, float wy)
{
    float cx, cy;
    enemy_centre(e, &cx, &cy);
    float dx = wx - cx, dy = wy - cy;
    return sqrtf(dx * dx + dy * dy);
}

/* Move the enemy towards (tx, ty) at the given speed this frame. */
static void move_towards(Enemy *e, float tx, float ty,
                         float speed, float dt)
{
    float cx, cy;
    enemy_centre(e, &cx, &cy);
    float dx = tx - cx, dy = ty - cy;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 1.0f) return;
    float step = speed * dt;
    if (step > dist) step = dist;
    float ratio = step / dist;
    e->x += dx * ratio;
    e->y += dy * ratio;
}

static void enemy_update_direction(Enemy *e, float dx, float dy)
{
    if (!e) return;
    float adx = fabsf(dx);
    float ady = fabsf(dy);
    if (adx >= ady) {
        if (dx > 0.0f) e->direction = ENEMY_DIR_RIGHT;
        else if (dx < 0.0f) e->direction = ENEMY_DIR_LEFT;
    } else {
        if (dy > 0.0f) e->direction = ENEMY_DIR_FORWARD;
        else if (dy < 0.0f) e->direction = ENEMY_DIR_BACKWARD;
    }
}

static int move_towards_animated(Enemy *e, float tx, float ty,
                                 float speed, float dt)
{
    float cx, cy;
    enemy_centre(e, &cx, &cy);
    float dx = tx - cx, dy = ty - cy;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 1.0f) return 0;

    enemy_update_direction(e, dx, dy);
    move_towards(e, tx, ty, speed, dt);
    return 1;
}

static void enemy_update_animation(Enemy *e, float dt)
{
    if (!e) return;
    if (e->is_moving) {
        animation_update(&e->move_anim, dt);
    } else {
        e->move_anim.current_frame = 0;
        e->move_anim.frame_timer = 0.0f;
    }
}

static SDL_Texture *load_enemy_frame(SDL_Renderer *renderer,
                                     const char *dir_name,
                                     const char *prefix,
                                     int frame_no)
{
    char path[512];
    int n = snprintf(path, sizeof(path),
             "assets/enemy/%s/%s%d.png", dir_name, prefix, frame_no);
    if (n < 0 || (size_t)n >= sizeof(path)) {
        SDL_Log("enemy: sprite path too long for %s/%s%d", dir_name, prefix, frame_no);
        return NULL;
    }
    return render_load_texture(renderer, path);
}

void enemy_load_sprites(Enemy *e, SDL_Renderer *renderer)
{
    if (!e || !renderer) return;

    e->forward_count = 0;
    e->backward_count = 0;
    e->left_count = 0;
    e->right_count = 0;

    /* Asset names are Indonesian:
       depan=forward, belakang=backward, kiri=left, kanan=right. */
    for (int i = 1; i <= ENEMY_MAX_ANIM_FRAMES; i++) {
        SDL_Texture *t = load_enemy_frame(renderer, "forward", "depan", i);
        if (!t) {
            SDL_Log("enemy: stopped loading forward frame %d", i);
            break;
        }
        e->forward_frames[e->forward_count++] = t;
    }
    for (int i = 1; i <= 6; i++) {
        SDL_Texture *t = load_enemy_frame(renderer, "backward", "belakang", i);
        if (!t) {
            SDL_Log("enemy: stopped loading backward frame %d", i);
            break;
        }
        e->backward_frames[e->backward_count++] = t;
    }
    for (int i = 1; i <= ENEMY_MAX_ANIM_FRAMES; i++) {
        SDL_Texture *t = load_enemy_frame(renderer, "left", "kiri", i);
        if (!t) {
            SDL_Log("enemy: stopped loading left frame %d", i);
            break;
        }
        e->left_frames[e->left_count++] = t;
    }
    for (int i = 1; i <= 6; i++) {
        SDL_Texture *t = load_enemy_frame(renderer, "right", "kanan", i);
        if (!t) {
            SDL_Log("enemy: stopped loading right frame %d", i);
            break;
        }
        e->right_frames[e->right_count++] = t;
    }
}

/* ── Update ───────────────────────────────────────────────────────────── */

void enemy_update(Enemy *e, float player_x, float player_y,
                  int player_in_room, float dt)
{
    if (!e || !e->active || !e->grid) return;
    e->is_moving = 0;

    float dist_to_player = player_in_room
        ? enemy_dist(e, player_x, player_y)
        : FLT_MAX;

    /* ── State transitions ── */
    if (e->state == ENEMY_STATE_PATROL) {
        if (player_in_room && dist_to_player <= ENEMY_CHASE_RADIUS) {
            e->state        = ENEMY_STATE_CHASE;
            e->path_len     = 0;
            e->path_idx     = 0;
            e->repath_timer = 0.0f; /* compute path immediately */
        }
    } else if (e->state == ENEMY_STATE_CHASE) {
        if (!player_in_room || dist_to_player > ENEMY_PATROL_RADIUS) {
            e->state = ENEMY_STATE_PATROL;
            e->path_len = 0;
        }
    } else {
        /* INACTIVE — should not reach here once active=1 */
        e->state = ENEMY_STATE_PATROL;
    }

    /* ── Patrol ── */
    if (e->state == ENEMY_STATE_PATROL) {
        if (e->waypoint_count <= 0) return;

        EnemyPoint *wp = &e->waypoints[e->current_waypoint];
        float cx, cy;
        enemy_centre(e, &cx, &cy);
        float dx = wp->x - cx, dy = wp->y - cy;
        float d  = sqrtf(dx * dx + dy * dy);

        if (d <= ENEMY_WAYPOINT_THRESH) {
            /* Reached this waypoint — advance */
            e->current_waypoint += e->patrol_dir;
            if (e->current_waypoint >= e->waypoint_count) {
                e->current_waypoint = e->waypoint_count - 2; /* bounce back */
                e->patrol_dir = -1;
            } else if (e->current_waypoint < 0) {
                e->current_waypoint = 1;                     /* bounce forward */
                e->patrol_dir = 1;
            }
        } else {
            e->is_moving = move_towards_animated(
                e, wp->x, wp->y, ENEMY_PATROL_SPEED, dt);
        }
        enemy_update_animation(e, dt);
        return;
    }

    /* ── Chase (A*) ── */
    if (e->state == ENEMY_STATE_CHASE) {
        /* Throttle A* recalculations */
        e->repath_timer -= dt;
        if (e->repath_timer <= 0.0f || e->path_len == 0) {
            e->repath_timer = ENEMY_REPATH_INTERVAL;

            /* Convert positions to grid cells */
            float cx, cy;
            enemy_centre(e, &cx, &cy);
            int sr = world_to_row(e, cy), sc = world_to_col(e, cx);
            int gr = world_to_row(e, player_y), gc = world_to_col(e, player_x);

            int new_len = 0;
            astar(e->grid, e->grid_rows, e->grid_cols,
                  sr, sc, gr, gc,
                  e->path, &new_len,
                  e->tile_w, e->tile_h);
            e->path_len = new_len;
            e->path_idx = 0;
        }

        if (e->path_len > 0 && e->path_idx < e->path_len) {
            EnemyPoint *node = &e->path[e->path_idx];
            float cx, cy;
            enemy_centre(e, &cx, &cy);
            float dx = node->x - cx, dy = node->y - cy;
            float d  = sqrtf(dx * dx + dy * dy);

            if (d <= ENEMY_WAYPOINT_THRESH) {
                e->path_idx++;
            } else {
                e->is_moving = move_towards_animated(
                    e, node->x, node->y, ENEMY_CHASE_SPEED, dt);
            }
        } else {
            /* Path exhausted — move directly towards player as fallback */
            e->is_moving = move_towards_animated(
                e, player_x, player_y, ENEMY_CHASE_SPEED, dt);
        }
        enemy_update_animation(e, dt);
    }
}

/* ── Hit detection ────────────────────────────────────────────────────── */

int enemy_hits_player(const Enemy *e, float player_x, float player_y)
{
    if (!e || !e->active) return 0;
    float cx, cy;
    enemy_centre(e, &cx, &cy);
    float dx = player_x - cx, dy = player_y - cy;
    float dist = sqrtf(dx * dx + dy * dy);
    return (dist <= ENEMY_HIT_DIST) ? 1 : 0;
}

/* ── Render ───────────────────────────────────────────────────────────── */

void enemy_render(const Enemy *e, SDL_Renderer *renderer, const Camera *cam)
{
    if (!e || !e->active || !renderer || !cam) return;

    int sx = camera_to_screen_x(cam, e->x);
    int sy = camera_to_screen_y(cam, e->y);

    const SDL_Texture *const *frames = NULL;
    int frame_count = 0;
    switch (e->direction) {
        case ENEMY_DIR_BACKWARD:
            frames = (const SDL_Texture *const *)e->backward_frames;
            frame_count = e->backward_count;
            break;
        case ENEMY_DIR_LEFT:
            frames = (const SDL_Texture *const *)e->left_frames;
            frame_count = e->left_count;
            break;
        case ENEMY_DIR_RIGHT:
            frames = (const SDL_Texture *const *)e->right_frames;
            frame_count = e->right_count;
            break;
        case ENEMY_DIR_FORWARD:
        default:
            frames = (const SDL_Texture *const *)e->forward_frames;
            frame_count = e->forward_count;
            break;
    }

    if (frames && frame_count > 0) {
        int idx = animation_get_frame(&e->move_anim) % frame_count;
        SDL_Texture *tex = (SDL_Texture *)frames[idx];
        if (tex) {
            SDL_FRect dst = { (float)sx, (float)sy, (float)ENEMY_W, (float)ENEMY_H };
            SDL_RenderTexture(renderer, tex, NULL, &dst);
            return;
        }
    }

    /* Fallback rectangle if textures are unavailable */
    render_filled_rect(renderer, sx, sy, ENEMY_W, ENEMY_H, 180, 20, 20, 255);
    render_rect_outline(renderer, sx, sy, ENEMY_W, ENEMY_H, 255, 60, 60, 255);
}
