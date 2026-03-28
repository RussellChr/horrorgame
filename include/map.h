#ifndef MAP_H
#define MAP_H

#include <SDL3/SDL.h>

#define TILE_SIZE   32
#define MAP_MAX_COLS 256
#define MAP_MAX_ROWS 256

typedef struct {
    int         cols;
    int         rows;
    int         tiles[MAP_MAX_ROWS][MAP_MAX_COLS]; /* -1 = floor, 0 = wall */
    SDL_Texture *tileset;
    int         tileset_cols; /* how many tiles wide the spritesheet is */
} Map;

/* Load CSV and spritesheet. Returns 1 on success, 0 on failure. */
int  map_load(Map *map, SDL_Renderer *renderer,
              const char *csv_path, const char *png_path,
              int tileset_cols);

void map_render(const Map *map, SDL_Renderer *renderer,
                float cam_x, float cam_y,
                int viewport_w, int viewport_h);

void map_free(Map *map);

/* Collision query: returns 1 if tile at world pixel (wx, wy) is a wall. */
int  map_is_wall(const Map *map, float wx, float wy);

#endif /* MAP_H */
