#ifndef TILEMAP_H
#define TILEMAP_H

#include "world.h"

/*
 * Load collision rectangles from a Tiled CSV export into a Location.
 *
 * filepath  - path to the CSV file (tile IDs, comma-separated rows)
 * loc       - Location to populate with colliders
 * tile_size - width and height of each tile in pixels (e.g. 32)
 *
 * Any tile with ID > 0 is treated as solid.  Consecutive solid tiles on the
 * same row are merged into one rectangle to keep the collider count low.
 *
 * Returns the number of colliders added, or -1 on error.
 */
int tilemap_load_colliders(const char *filepath, Location *loc, int tile_size);

#endif /* TILEMAP_H */
