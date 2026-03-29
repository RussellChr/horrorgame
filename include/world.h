#ifndef WORLD_H
#define WORLD_H

#include <SDL3/SDL.h>
#include "collision.h"
#include "camera.h"

#define MAX_LOCATIONS      16
#define MAX_EXITS           8
#define MAX_DECOR          32

#define LOCATION_NAME_MAX  64
#define LOCATION_DESC_MAX  512

/* ── Room dimensions ──────────────────────────────────────────────────── */

#define ROOM_W  2560   /* world-space width  (2 × viewport)         */
#define ROOM_H   720   /* world-space height (== viewport)          */
#define FLOOR_Y  580   /* y of the walkable floor surface           */

/* ── Entrance Hall interactive Stranger NPC world position ───────────── */

#define STRANGER_NPC_X  820  /* centre-left x of the 30-px body    */

/* ── Exit (logical, for text-game movement) ──────────────────────────── */

typedef struct {
    char direction[16];
    int  target_id;
    int  locked;
    int  required_item_id;
} Exit;

/* ── Decorative element (drawn as a coloured rectangle or texture) ───── */

typedef struct {
    int   x, y, w, h;
    Uint8 r, g, b;
    char  label[32];         /* optional label painted on/above the object */
    SDL_Texture *texture;    /* if non-NULL, rendered instead of the rect  */
    int   hidden;            /* if non-zero, skip rendering this decor     */
} Decor;

/* ── Location ─────────────────────────────────────────────────────────── */

typedef struct {
    /* Logical data */
    int  id;
    char name[LOCATION_NAME_MAX];
    char description[LOCATION_DESC_MAX];
    char atmosphere[LOCATION_DESC_MAX];
    int  visited;
    int  item_id;
    int  item_taken;
    int  is_danger_zone;

    /* Exits (logical navigation) */
    Exit exits[MAX_EXITS];
    int  exit_count;

    /* Visual: background colours */
    Uint8 wall_r, wall_g, wall_b;
    Uint8 floor_r, floor_g, floor_b;
    Uint8 ceil_r,  ceil_g,  ceil_b;

    /* Visual background texture */
    SDL_Texture *background_texture;

    /* Visual decorations */
    Decor decor[MAX_DECOR];
    int   decor_count;

    /* Collision rectangles */
    Rect  colliders[MAX_COLLISION_RECTS];
    int   collider_count;

    /* Interactive trigger zones */
    TriggerZone triggers[MAX_TRIGGER_ZONES];
    int          trigger_count;

    /* Player spawn position when entering this location */
    float spawn_x, spawn_y;
    float texture_scale;

    /* Dynamic room dimensions (set from background texture at load time) */
    int room_width;
    int room_height;
} Location;

/* ── World ────────────────────────────────────────────────────────────── */

typedef struct {
    Location locations[MAX_LOCATIONS];
    int      location_count;
} World;

/* ── API ──────────────────────────────────────────────────────────────── */

World    *world_create(void);
void      world_destroy(World *world);

Location *world_get_location(World *world, int id);
Location *world_get_location_by_name(World *world, const char *name);

/* Text-game movement (kept for story logic) */
void world_describe_location(const Location *loc);
int  world_move(World *world, int current_id,
                const char *direction, int *new_id);
void world_add_exit(Location *loc, const char *direction,
                    int target_id, int locked, int required_item_id);

/* File loading (logical data only) */
int world_load_locations(World *world, const char *filepath);

/* Visual room setup – builds decor, colliders and triggers procedurally. */
void world_setup_rooms(World *world, SDL_Renderer *renderer);

/* Draw the current room. */
void world_render_room(const Location *loc, SDL_Renderer *renderer,
                       const Camera *cam);


#endif /* WORLD_H */
