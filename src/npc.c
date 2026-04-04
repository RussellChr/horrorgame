#include "npc.h"
#include "utils.h"
#include "render.h"
#include "world.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define NPC_SPEED      150.0f
#define NPC_W           20
#define NPC_H           40
#define PATROL_SPEED    80.0f
#define PATROL_ARRIVE   8.0f

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
    for (int i = 0; i < manager->npc_count; i++)
        npc_destroy(&manager->npcs[i]);
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
    npc->h = NPC_H;
    
    /* Setup collider */
    npc->collider.x = x;
    npc->collider.y = y;
    npc->collider.w = NPC_W;
    npc->collider.h = NPC_H;

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

static void npc_patrol_update(NPC *npc, float delta_time)
{
    if (!npc || npc->patrol_wp_count < 2) return;

    PatrolWaypoint *target = &npc->patrol_waypoints[npc->patrol_wp_index];
    float dx   = target->x - npc->x;
    float dy   = target->y - npc->y;
    float dist = sqrtf(dx * dx + dy * dy);

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
    float inv = 1.0f / dist;
    npc->vx = dx * inv * PATROL_SPEED;
    npc->vy = dy * inv * PATROL_SPEED;
    npc->facing_right  = (dx > 0) ? 1 : 0;
    npc->current_state = NPC_STATE_WALKING;

    (void)delta_time;
}

/* ── Update ────────────────────────────────────────────────────────────── */

void npc_update(NPC *npc, float delta_time)
{
    if (!npc) return;

    /* Drive patrol movement before applying velocity */
    if (npc->has_patrol)
        npc_patrol_update(npc, delta_time);

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

    /* Simple colored rectangle for NPC sprite */
    if (npc->npc_type == NPC_TYPE_FRIENDLY)
        render_filled_rect(renderer, sx, sy, npc->w, npc->h,
                          100, 200, 100, 255);  /* Green */
    else if (npc->npc_type == NPC_TYPE_HOSTILE)
        render_filled_rect(renderer, sx, sy, npc->w, npc->h,
                          200, 50, 50, 255);    /* Red */
    else
        render_filled_rect(renderer, sx, sy, npc->w, npc->h,
                          150, 150, 150, 255);  /* Gray */

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
