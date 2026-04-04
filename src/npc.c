#include "npc.h"
#include "utils.h"
#include "render.h"
#include "world.h"
#include "map.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define NPC_SPEED      150.0f
#define NPC_W           20
#define NPC_HEIGHT      40
#define PATROL_SPEED    80.0f
#define PATROL_ARRIVE   8.0f
#define CHASE_SPEED     160.0f
#define CHASE_ARRIVE    12.0f
#define CHASE_REPATH_INTERVAL  0.5f   /* seconds between A* recomputes */

/* ── A* Pathfinding (internal) ─────────────────────────────────────────── */

#define ASTAR_MAX_CELLS 4096

typedef struct {
    float g;       /* cost from start */
    int   parent;  /* flat index of parent cell, -1 for start */
    int   open;
    int   closed;
} AStarCell;

/* Static storage — avoids per-frame heap allocation (single-threaded). */
static AStarCell s_astar[ASTAR_MAX_CELLS];

/* Manhattan-distance heuristic scaled to tile size. */
static float astar_h(int r, int c, int er, int ec, float tile_w, float tile_h)
{
    float scale = tile_w < tile_h ? tile_w : tile_h;
    return (float)(abs(r - er) + abs(c - ec)) * scale;
}

/*
 * Run A* on a tile grid from (sr,sc) to (er,ec).
 * walkable = cell != MAP_TILE_WALL (0).
 * Returns the number of world-space waypoints written into out_x/out_y,
 * or 0 if no path is found.
 */
static int astar_find(const int *cells, int rows, int cols,
                      int sr, int sc, int er, int ec,
                      float tile_w, float tile_h,
                      float *out_x, float *out_y, int max_out)
{
    int n = rows * cols;
    if (n > ASTAR_MAX_CELLS || n <= 0) return 0;

    /* Clamp endpoints to grid. */
    if (sr < 0) sr = 0; if (sr >= rows) sr = rows - 1;
    if (sc < 0) sc = 0; if (sc >= cols) sc = cols - 1;
    if (er < 0) er = 0; if (er >= rows) er = rows - 1;
    if (ec < 0) ec = 0; if (ec >= cols) ec = cols - 1;

    /* If goal is a wall, search for the nearest walkable cell. */
    if (cells[er * cols + ec] == MAP_TILE_WALL) {
        int best = -1; float best_d = 1e30f;
        for (int i = 0; i < n; i++) {
            if (cells[i] == MAP_TILE_WALL) continue;
            int r = i / cols, c = i % cols;
            float d = astar_h(r, c, er, ec, tile_w, tile_h);
            if (d < best_d) { best_d = d; best = i; }
        }
        if (best < 0) return 0;
        er = best / cols; ec = best % cols;
    }

    /* Initialise nodes. */
    for (int i = 0; i < n; i++) {
        s_astar[i].g      = 1e30f;
        s_astar[i].parent = -1;
        s_astar[i].open   = 0;
        s_astar[i].closed = 0;
    }

    int start = sr * cols + sc;
    int goal  = er * cols + ec;
    s_astar[start].g    = 0.0f;
    s_astar[start].open = 1;

    static const int dr[4] = {-1,  1,  0, 0};
    static const int dc[4] = { 0,  0, -1, 1};

    int found = 0;
    for (;;) {
        /* Pick open cell with smallest f = g + h. */
        int   curr   = -1;
        float best_f = 1e30f;
        for (int i = 0; i < n; i++) {
            if (!s_astar[i].open) continue;
            int r = i / cols, c = i % cols;
            float f = s_astar[i].g + astar_h(r, c, er, ec, tile_w, tile_h);
            if (f < best_f) { best_f = f; curr = i; }
        }
        if (curr < 0) break;   /* no path */

        if (curr == goal) { found = 1; break; }

        s_astar[curr].open   = 0;
        s_astar[curr].closed = 1;
        int cr = curr / cols;
        int cc = curr % cols;

        for (int d = 0; d < 4; d++) {
            int nr = cr + dr[d];
            int nc = cc + dc[d];
            if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
            if (cells[nr * cols + nc] == MAP_TILE_WALL) continue;
            int ni = nr * cols + nc;
            if (s_astar[ni].closed) continue;
            float step = (dr[d] == 0) ? tile_w : tile_h;
            float ng   = s_astar[curr].g + step;
            if (ng < s_astar[ni].g) {
                s_astar[ni].g      = ng;
                s_astar[ni].parent = curr;
                s_astar[ni].open   = 1;
            }
        }
    }

    if (!found) return 0;

    /* Reconstruct path in reverse into a temporary stack. */
    static int tmp_path[ASTAR_MAX_CELLS];
    int tmp_len = 0;
    int idx = goal;
    while (idx >= 0 && tmp_len < ASTAR_MAX_CELLS) {
        tmp_path[tmp_len++] = idx;
        idx = s_astar[idx].parent;
    }

    /* Convert to world-space waypoints (reversed so path goes start→goal). */
    int copy = tmp_len < max_out ? tmp_len : max_out;
    for (int i = 0; i < copy; i++) {
        int cell = tmp_path[tmp_len - 1 - i];
        out_x[i] = ((cell % cols) + 0.5f) * tile_w;
        out_y[i] = ((cell / cols) + 0.5f) * tile_h;
    }
    return copy;
}

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

NPCManager *npc_manager_create(void)
{
    NPCManager *mgr = calloc(1, sizeof(NPCManager));
    if (!mgr) return NULL;
    mgr->npc_count = 0;
    return mgr;
}

void npc_manager_destroy(NPCManager *manager)
{
    if (!manager) return;
    /* NPCs are value-copied into the manager's embedded array via
     * npc_manager_add(), so they are NOT individually heap-allocated.
     * Calling free() on &manager->npcs[i] would free a pointer into
     * the middle of the manager allocation — undefined behaviour and
     * the direct cause of the "pointer being freed was not allocated"
     * crash on exit.  The NPC struct holds no pointer-typed resources
     * of its own, so simply freeing the manager block is sufficient. */
    free(manager);
}

NPC *npc_create(int id, const char *name, float x, float y, int npc_type)
{
    NPC *npc = calloc(1, sizeof(NPC));
    if (!npc) return NULL;

    npc->id = id;
    strncpy(npc->name, name ? name : "Unknown", NPC_NAME_MAX - 1);
    npc->x = x;
    npc->y = y;
    npc->npc_type = npc_type;
    npc->current_state = NPC_STATE_IDLE;
    npc->facing_right = 1;
    npc->interaction_range = 80.0f;
    npc->location_id = 0;
    npc->w = NPC_W;
    npc->h = NPC_HEIGHT;
    
    /* Setup collider */
    npc->collider.x = x;
    npc->collider.y = y;
    npc->collider.w = NPC_W;
    npc->collider.h = NPC_HEIGHT;

    /* Initialize animations */
    animation_init(&npc->idle_anim, 2, 1.5f, 1);
    animation_init(&npc->walk_anim, 4, 8.0f, 1);
    animation_init(&npc->talk_anim, 3, 2.0f, 1);

    return npc;
}

void npc_destroy(NPC *npc)
{
    free(npc);
}

int npc_manager_add(NPCManager *manager, NPC *npc)
{
    if (!manager || !npc || manager->npc_count >= MAX_NPCS)
        return 0;
    
    manager->npcs[manager->npc_count++] = *npc;
    return 1;
}

NPC *npc_manager_get(NPCManager *manager, int npc_id)
{
    if (!manager) return NULL;
    for (int i = 0; i < manager->npc_count; i++)
        if (manager->npcs[i].id == npc_id)
            return &manager->npcs[i];
    return NULL;
}

void npc_manager_remove(NPCManager *manager, int npc_id)
{
    if (!manager) return;
    for (int i = 0; i < manager->npc_count; i++) {
        if (manager->npcs[i].id == npc_id) {
            for (int j = i; j < manager->npc_count - 1; j++)
                manager->npcs[j] = manager->npcs[j + 1];
            manager->npc_count--;
            return;
        }
    }
}

/* ── Patrol waypoint update (internal) ─────────────────────────────────── */

static void npc_patrol_update(NPC *npc)
{
    if (!npc || npc->patrol_wp_count < 2) return;

    PatrolWaypoint *target = &npc->patrol_waypoints[npc->patrol_wp_index];
    float dx   = target->x - npc->x;
    float dy   = target->y - npc->y;
    float dist = sqrtf(dx * dx + dy * dy);

    /* Guard against degenerate near-zero distance before dividing */
    if (dist < PATROL_ARRIVE) {
        /* Arrived — advance to next waypoint, reversing at the ends */
        if (npc->patrol_state == PATROL_STATE_FORWARD) {
            if (npc->patrol_wp_index < npc->patrol_wp_count - 1) {
                npc->patrol_wp_index++;
            } else {
                npc->patrol_state = PATROL_STATE_BACKWARD;
                npc->patrol_wp_index--;
            }
        } else {
            if (npc->patrol_wp_index > 0) {
                npc->patrol_wp_index--;
            } else {
                npc->patrol_state = PATROL_STATE_FORWARD;
                npc->patrol_wp_index++;
            }
        }
        npc->vx = 0.0f;
        npc->vy = 0.0f;
        return;
    }

    /* Steer towards the current waypoint */
    float inv_dist = 1.0f / dist;
    npc->vx = dx * inv_dist * PATROL_SPEED;
    npc->vy = dy * inv_dist * PATROL_SPEED;
    npc->facing_right  = (dx > 0) ? 1 : 0;
    npc->current_state = NPC_STATE_WALKING;
}

/* ── Update ────────────────────────────────────────────────────────────── */

void npc_update(NPC *npc, float delta_time)
{
    if (!npc) return;

    /* Drive patrol movement before applying velocity.
     * Skip patrol steering when the NPC is actively chasing. */
    if (npc->has_patrol && npc->current_state != NPC_STATE_CHASING)
        npc_patrol_update(npc);

    switch (npc->current_state) {
        case NPC_STATE_IDLE:
            npc->is_moving = 0;
            break;

        case NPC_STATE_WALKING:
            if (npc->vx != 0.0f || npc->vy != 0.0f) {
                npc->x += npc->vx * delta_time;
                npc->y += npc->vy * delta_time;
                npc->collider.x = npc->x;
                npc->collider.y = npc->y;
                npc->is_moving = 1;
            } else {
                npc->current_state = NPC_STATE_IDLE;
            }
            break;

        case NPC_STATE_CHASING:
            /* Chase velocity is set by npc_update_chase(); apply it here. */
            npc->x += npc->vx * delta_time;
            npc->y += npc->vy * delta_time;
            npc->collider.x = npc->x;
            npc->collider.y = npc->y;
            npc->is_moving = 1;
            break;

        case NPC_STATE_TALKING:
            npc->is_moving = 0;
            break;

        case NPC_STATE_FOLLOWING:
            /* Placeholder for follow behavior */
            npc->is_moving = 1;
            break;

        default:
            break;
    }

    /* Update animation */
    if (npc->is_moving)
        animation_update(&npc->walk_anim, delta_time);
    else
        animation_update(&npc->idle_anim, delta_time);
}

void npc_manager_update(NPCManager *manager, float delta_time)
{
    if (!manager) return;
    for (int i = 0; i < manager->npc_count; i++)
        npc_update(&manager->npcs[i], delta_time);
}

/* ── Rendering ────────────────────────────────────────────────────────── */

void npc_render(NPC *npc, SDL_Renderer *renderer, const Camera *cam)
{
    if (!npc || !renderer) return;

    int sx = cam ? camera_to_screen_x(cam, npc->x) : (int)npc->x;
    int sy = cam ? camera_to_screen_y(cam, npc->y) : (int)npc->y;

    /* Colour varies by type and chase state. */
    if (npc->npc_type == NPC_TYPE_HOSTILE) {
        if (npc->current_state == NPC_STATE_CHASING)
            render_filled_rect(renderer, sx, sy, npc->w, npc->h,
                               255, 80, 0, 255);   /* bright orange — chasing */
        else
            render_filled_rect(renderer, sx, sy, npc->w, npc->h,
                               200, 50, 50, 255);  /* dark red — patrolling   */
    } else if (npc->npc_type == NPC_TYPE_FRIENDLY) {
        render_filled_rect(renderer, sx, sy, npc->w, npc->h,
                           100, 200, 100, 255);
    } else {
        render_filled_rect(renderer, sx, sy, npc->w, npc->h,
                           150, 150, 150, 255);
    }

    /* Draw name above NPC */
    render_text_centered(renderer, npc->name,
                        sx + npc->w / 2, sy - 20,
                        1, 255, 255, 255);
}

void npc_manager_render(NPCManager *manager, SDL_Renderer *renderer, const Camera *cam)
{
    if (!manager || !renderer) return;
    for (int i = 0; i < manager->npc_count; i++)
        npc_render(&manager->npcs[i], renderer, cam);
}

/* ── Chase (A*) update ─────────────────────────────────────────────────── */

void npc_update_chase(NPC *npc, float delta_time,
                      float player_x, float player_y,
                      const int *nav_cells, int nav_rows, int nav_cols,
                      int room_w, int room_h)
{
    if (!npc) return;

    npc->current_state = NPC_STATE_CHASING;
    npc->chase_repath_timer -= delta_time;

    if (npc->chase_repath_timer <= 0.0f) {
        npc->chase_repath_timer = CHASE_REPATH_INTERVAL;

        if (nav_cells && nav_rows > 0 && nav_cols > 0) {
            float tile_w = (float)room_w / (float)nav_cols;
            float tile_h = (float)room_h / (float)nav_rows;

            int sc = (int)(npc->x   / tile_w);
            int sr = (int)(npc->y   / tile_h);
            int ec = (int)(player_x / tile_w);
            int er = (int)(player_y / tile_h);

            npc->chase_path_len = astar_find(
                nav_cells, nav_rows, nav_cols,
                sr, sc, er, ec,
                tile_w, tile_h,
                npc->chase_path_x, npc->chase_path_y,
                MAX_CHASE_PATH);
        } else {
            /* No nav grid: aim directly at the player. */
            npc->chase_path_x[0] = player_x;
            npc->chase_path_y[0] = player_y;
            npc->chase_path_len  = 1;
        }
        npc->chase_path_idx = 0;
    }

    /* Advance path index when close enough to the current waypoint. */
    while (npc->chase_path_idx < npc->chase_path_len) {
        float tx  = npc->chase_path_x[npc->chase_path_idx];
        float ty  = npc->chase_path_y[npc->chase_path_idx];
        float dx  = tx - npc->x;
        float dy  = ty - npc->y;
        float d2  = dx * dx + dy * dy;
        if (d2 > CHASE_ARRIVE * CHASE_ARRIVE) break;
        npc->chase_path_idx++;
    }

    /* Steer toward the current path node. */
    if (npc->chase_path_idx < npc->chase_path_len) {
        float tx   = npc->chase_path_x[npc->chase_path_idx];
        float ty   = npc->chase_path_y[npc->chase_path_idx];
        float dx   = tx - npc->x;
        float dy   = ty - npc->y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > 0.01f) {
            float inv_dist = 1.0f / dist;
            npc->vx    = dx * inv_dist * CHASE_SPEED;
            npc->vy    = dy * inv_dist * CHASE_SPEED;
            npc->facing_right = (dx > 0) ? 1 : 0;
        }
    } else {
        /* End of path: head directly toward the player. */
        float dx   = player_x - npc->x;
        float dy   = player_y - npc->y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > 0.01f) {
            npc->vx = dx / dist * CHASE_SPEED;
            npc->vy = dy / dist * CHASE_SPEED;
            npc->facing_right = (dx > 0) ? 1 : 0;
        } else {
            npc->vx = 0.0f;
            npc->vy = 0.0f;
        }
    }
}

/* ── Behavior ──────────────────────────────────────────────────────────── */

void npc_set_patrol_waypoints(NPC *npc, const PatrolWaypoint *wps, int count)
{
    if (!npc || !wps || count <= 0) return;
    if (count > MAX_PATROL_WAYPOINTS) count = MAX_PATROL_WAYPOINTS;

    for (int i = 0; i < count; i++)
        npc->patrol_waypoints[i] = wps[i];
    npc->patrol_wp_count = count;
    npc->patrol_wp_index = 0;
    npc->patrol_state    = PATROL_STATE_FORWARD;
    npc->has_patrol      = 1;

    /* Start moving towards the first waypoint */
    npc->current_state = NPC_STATE_WALKING;
}

void npc_set_patrol(NPC *npc, float x1, float x2)
{
    if (!npc) return;
    npc->patrol_x1 = x1;
    npc->patrol_x2 = x2;
}

void npc_move_towards(NPC *npc, float target_x, float delta_time)
{
    if (!npc) return;

    float dx = target_x - npc->x;
    if (fabs(dx) < 5.0f) {
        npc->vx = 0.0f;
        npc->current_state = NPC_STATE_IDLE;
        return;
    }

    npc->facing_right = (dx > 0) ? 1 : 0;
    npc->vx = (dx > 0) ? NPC_SPEED : -NPC_SPEED;
    npc->current_state = NPC_STATE_WALKING;
}

int npc_check_player_interaction(NPC *npc, float player_x, float player_y)
{
    if (!npc) return 0;

    float dx = player_x - npc->x;
    float dy = player_y - npc->y;
    float dist = sqrtf(dx*dx + dy*dy);

    return (dist <= npc->interaction_range) ? 1 : 0;
}
