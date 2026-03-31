#ifndef MAP_H
#define MAP_H

#include "world.h"   /* Location, ROOM_W, ROOM_H */

/* ── Tile values used in the CSV ─────────────────────────────────────── */
#define MAP_TILE_WALL   0    /* impassable */
#define MAP_TILE_FLOOR (-1)  /* walkable   */
#define MAP_TILE_DOOR   1    /* entrance / exit door */

/* ── Map ─────────────────────────────────────────────────────────────── */

typedef struct {
    int  *cells;   /* flat array [row * cols + col] */
    int   rows;
    int   cols;
} Map;

/* ── API ─────────────────────────────────────────────────────────────── */

/*
 * Load a CSV tile map from 'filepath'.
 * Returns a heap-allocated Map on success, or NULL on failure.
 * The caller must free it with map_free().
 */
Map *map_load_csv(const char *filepath);

/* Free a Map returned by map_load_csv(). */
void map_free(Map *map);

/*
 * Build collision rectangles for every wall run in 'map' and add them to
 * 'loc->colliders'.  The tiles are scaled so that the full grid maps onto
 * [0, ROOM_W) × [0, ROOM_H) in world space.
 *
 * Adjacent wall tiles on the same row are merged into a single rectangle
 * to keep the collider count manageable.
 *
 * Returns the number of colliders added, or -1 if loc is NULL.
 */
int map_build_colliders(const Map *map, Location *loc);

/*
 * Build exit trigger zones for every run of 'tile_value' tiles in 'map'.
 * Adjacent matching tiles on the same row are merged into a single TriggerZone.
 * tile_value           : the CSV tile value that represents a door (e.g. MAP_TILE_DOOR or 4).
 * dest_id              : location ID the player is transported to.
 * dest_spawn_x/y       : player spawn coordinates in the destination room.
 * Returns the number of trigger zones added.
 */
int map_build_door_triggers(const Map *map, Location *loc,
                            int tile_value, int dest_id,
                            float dest_spawn_x, float dest_spawn_y);

/*
 * Find the world-space position of the first floor tile at or near
 * (hint_row, hint_col), searching outward in a spiral.
 * Writes the spawn coordinates into *out_x (tile centre x) and
 * *out_y (tile bottom edge y, matching the player's centre-bottom origin).
 * Returns 1 on success, 0 if no floor tile was found.
 */
int map_find_spawn(const Map *map, int hint_row, int hint_col,
                   float *out_x, float *out_y, int room_w, int room_h);

#endif /* MAP_H */
