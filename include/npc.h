#ifndef NPC_H
#define NPC_H

#include <SDL3/SDL.h>
#include "animation.h"
#include "collision.h"
#include "camera.h"

#define NPC_NAME_MAX          64
#define MAX_NPCS              16
#define MAX_PATROL_WAYPOINTS   8

/* ── NPC Types/Roles ──────────────────────────────────────────────────── */

typedef enum {
    NPC_TYPE_FRIENDLY,
    NPC_TYPE_HOSTILE,
    NPC_TYPE_NEUTRAL
} NPCType;

/* ── NPC Behavior States ──────────────────────────────────────────────── */

typedef enum {
    NPC_STATE_IDLE,
    NPC_STATE_WALKING,
    NPC_STATE_TALKING,
    NPC_STATE_FOLLOWING,
    NPC_STATE_ATTACKING
} NPCState;

/* ── Patrol State ─────────────────────────────────────────────────────── */

typedef enum {
    PATROL_STATE_FORWARD,
    PATROL_STATE_BACKWARD
} PatrolState;

/* ── Patrol Waypoint ──────────────────────────────────────────────────── */

typedef struct {
    float x, y;
} PatrolWaypoint;

/* ── NPC Structure ────────────────────────────────────────────────────── */

typedef struct {
    /* Identity & properties */
    int   id;
    char  name[NPC_NAME_MAX];
    int   npc_type;           /* NPCType enum */
    int   current_state;      /* NPCState enum */
    int   location_id;        /* room this NPC belongs to */

    /* Physical properties */
    float x, y;               /* world-space position */
    float vx, vy;             /* velocity in pixels/second */
    int   w, h;               /* sprite/rect size */
    int   facing_right;       /* 1 = right, 0 = left */
    int   is_moving;
    
    /* Collider */
    Rect  collider;
    
    /* Animations */
    Animation idle_anim;
    Animation walk_anim;
    Animation talk_anim;
    
    /* Interaction */
    int   dialogue_tree_id;   /* reference to dialogue tree */
    int   can_interact;       /* 1 if player is in interaction range */
    float interaction_range;  /* pixels within which player can trigger */
    
    /* AI/Behavior */
    float patrol_x1, patrol_x2;  /* patrol bounds (legacy 1D) */
    float move_timer;            /* timer for AI decisions */
    int   target_id;             /* ID of target (player or another NPC) */

    /* Waypoint patrol */
    PatrolWaypoint patrol_waypoints[MAX_PATROL_WAYPOINTS];
    int   patrol_wp_count;    /* number of valid waypoints */
    int   patrol_wp_index;    /* index of current target waypoint */
    int   patrol_state;       /* PatrolState enum */
    int   has_patrol;         /* 1 if waypoint patrol is active */
} NPC;

/* ── NPC Manager (store in Game struct) ────────────────────────────── */

typedef struct {
    NPC   npcs[MAX_NPCS];
    int   npc_count;
} NPCManager;

/* ── API ───────────────────────────────────────────────────────────────── */

/* Lifecycle */
NPCManager *npc_manager_create(void);
void        npc_manager_destroy(NPCManager *manager);

/* NPC operations */
NPC        *npc_create(int id, const char *name, float x, float y, int npc_type);
void        npc_destroy(NPC *npc);
int         npc_manager_add(NPCManager *manager, NPC *npc);
NPC        *npc_manager_get(NPCManager *manager, int npc_id);
void        npc_manager_remove(NPCManager *manager, int npc_id);

/* Per-frame updates */
void npc_update(NPC *npc, float delta_time);
void npc_manager_update(NPCManager *manager, float delta_time);

/* Rendering */
void npc_render(NPC *npc, SDL_Renderer *renderer, const Camera *cam);
void npc_manager_render(NPCManager *manager, SDL_Renderer *renderer, const Camera *cam);

/* Behavior/AI */
void npc_set_patrol(NPC *npc, float x1, float x2);
void npc_set_patrol_waypoints(NPC *npc, const PatrolWaypoint *wps, int count);
void npc_move_towards(NPC *npc, float target_x, float delta_time);
int  npc_check_player_interaction(NPC *npc, float player_x, float player_y);

#endif /* NPC_H */
