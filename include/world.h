#ifndef WORLD_H
#define WORLD_H

#define LOCATION_NAME_MAX  64
#define LOCATION_DESC_MAX  512
#define MAX_LOCATIONS      64
#define MAX_EXITS          8

/* ── Exit ──────────────────────────────────────────────────────────────── */

typedef struct {
    char direction[16];     /* "north", "south", "east", "west", "up", … */
    int  target_id;         /* ID of the destination Location             */
    int  locked;            /* 1 = requires an item to unlock             */
    int  required_item_id;  /* item that unlocks this exit (0 = none)     */
} Exit;

/* ── Location ──────────────────────────────────────────────────────────── */

typedef struct {
    int  id;
    char name[LOCATION_NAME_MAX];
    char description[LOCATION_DESC_MAX];
    char atmosphere[LOCATION_DESC_MAX]; /* extra flavour shown on revisit  */
    int  visited;           /* 1 after the player has been here           */
    int  exit_count;
    Exit exits[MAX_EXITS];
    int  item_id;           /* item found in this room (0 = none)         */
    int  item_taken;        /* 1 once the player picks it up              */
    int  is_danger_zone;    /* 1 triggers a sanity/health event           */
} Location;

/* ── World ─────────────────────────────────────────────────────────────── */

typedef struct {
    Location locations[MAX_LOCATIONS];
    int      location_count;
} World;

/* ── API ───────────────────────────────────────────────────────────────── */

World    *world_create(void);
void      world_destroy(World *world);

int       world_load_locations(World *world, const char *filepath);

Location *world_get_location(World *world, int id);
Location *world_get_location_by_name(World *world, const char *name);

void      world_describe_location(const Location *loc);
int       world_move(World *world, int current_id,
                     const char *direction, int *new_id);

void      world_add_exit(Location *loc, const char *direction,
                         int target_id, int locked, int required_item_id);

#endif /* WORLD_H */
