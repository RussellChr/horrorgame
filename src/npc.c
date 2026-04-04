#include "npc.h"
#include "utils.h"
#include "render.h"
#include "world.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define NPC_SPEED 150.0f
#define NPC_W     20
#define NPC_SPRITE_H     40

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
    
    /* Setup collider */
    npc->collider.x = x;
    npc->collider.y = y;
    npc->collider.w = NPC_W;
    npc->collider.h = NPC_SPRITE_H;

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

/* ── Update ────────────────────────────────────────────────────────────── */

void npc_update(NPC *npc, float delta_time)
{
    if (!npc) return;

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

void npc_render(NPC *npc, SDL_Renderer *renderer)
{
    if (!npc || !renderer) return;

    /* Simple colored rectangle for NPC sprite */
    int color;
    if (npc->npc_type == NPC_TYPE_FRIENDLY)
        render_filled_rect(renderer, (int)npc->x, (int)npc->y, NPC_W, NPC_SPRITE_H,
                          100, 200, 100, 255);  /* Green */
    else if (npc->npc_type == NPC_TYPE_HOSTILE)
        render_filled_rect(renderer, (int)npc->x, (int)npc->y, NPC_W, NPC_SPRITE_H,
                          200, 100, 100, 255);  /* Red */
    else
        render_filled_rect(renderer, (int)npc->x, (int)npc->y, NPC_W, NPC_SPRITE_H,
                          150, 150, 150, 255);  /* Gray */

    /* Draw name above NPC */
    render_text_centered(renderer, npc->name,
                        (int)(npc->x + NPC_W/2), (int)(npc->y - 20),
                        1, 255, 255, 255);
}

void npc_manager_render(NPCManager *manager, SDL_Renderer *renderer)
{
    if (!manager || !renderer) return;
    for (int i = 0; i < manager->npc_count; i++)
        npc_render(&manager->npcs[i], renderer);
}

/* ── Behavior ──────────────────────────────────────────────────────────── */

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
