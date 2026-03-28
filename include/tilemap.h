#ifndef TILEMAP_H
#define TILEMAP_H

#include <SDL3/SDL.h>

#define TILE_SIZE 32

/* ── Tilemap ──────────────────────────────────────────────────────────────
 *
 * Stores a 2-D grid of integer tile IDs loaded from a CSV file.
 *
 * Tile ID conventions:
 *   -1  = floor (walkable, rendered as floor colour)
 *    0  = wall  (solid, rendered as first tileset tile or wall colour)
 *   >0  = other tileset tiles
 */

typedef struct {
    int cols;
    int rows;
    int tile_size;       /* pixels per tile (default: TILE_SIZE)            */
    int *tiles;          /* flattened row-major array [rows * cols]          */
    SDL_Texture *tileset;/* tile atlas texture; may be NULL                  */
    int tileset_cols;    /* number of tile columns in the tileset image      */
} Tilemap;

/* ── API ──────────────────────────────────────────────────────────────── */

Tilemap *tilemap_create(int cols, int rows);
void     tilemap_destroy(Tilemap *map);

/* Parse a CSV file (comma-separated integers) and fill the tile grid. */
int      tilemap_load_csv(Tilemap *map, const char *filepath);

int      tilemap_get_tile(const Tilemap *map, int col, int row);
void     tilemap_set_tile(Tilemap *map, int col, int row, int tile_id);

/* Render the tilemap using camera_x/camera_y as world-space offset. */
void     tilemap_render(const Tilemap *map, SDL_Renderer *renderer,
                        int camera_x, int camera_y);

#endif /* TILEMAP_H */
