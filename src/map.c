#include "map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <SDL3_image/SDL_image.h>

/* ── Internal: tile index → source rect on spritesheet ─────────────────── */
static SDL_FRect tile_src(const Map *map, int tile_id)
{
    /* Map CSV values to spritesheet indices:
       -1 (floor) → spritesheet index 0
        0 (wall)  → spritesheet index 1
       Extend this mapping if you add more tile types. */
    int sheet_idx;
    if      (tile_id == -1) sheet_idx = 0;   /* floor */
    else if (tile_id ==  0) sheet_idx = 1;   /* wall  */
    else                    sheet_idx = 0;   /* fallback */

    SDL_FRect src;
    src.x = (float)((sheet_idx % map->tileset_cols) * TILE_SIZE);
    src.y = (float)((sheet_idx / map->tileset_cols) * TILE_SIZE);
    src.w = (float)TILE_SIZE;
    src.h = (float)TILE_SIZE;
    return src;
}

/* ── map_load ───────────────────────────────────────────────────────────── */
int map_load(Map *map, SDL_Renderer *renderer,
             const char *csv_path, const char *png_path,
             int tileset_cols)
{
    if (!map || !renderer || !csv_path || !png_path) return 0;
    memset(map, 0, sizeof(*map));
    map->tileset_cols = tileset_cols;

    /* --- Load spritesheet --- */
    SDL_Surface *surf = IMG_Load(png_path);
    if (!surf) {
        SDL_Log("map_load: IMG_Load failed for '%s': %s", png_path, SDL_GetError());
        return 0;
    }
    map->tileset = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    if (!map->tileset) {
        SDL_Log("map_load: SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
        return 0;
    }

    /* --- Parse CSV --- */
    FILE *f = fopen(csv_path, "r");
    if (!f) {
        SDL_Log("map_load: cannot open '%s'", csv_path);
        SDL_DestroyTexture(map->tileset);
        map->tileset = NULL;
        return 0;
    }

    char line[MAP_MAX_COLS * 6];
    int row = 0;
    while (fgets(line, sizeof(line), f) && row < MAP_MAX_ROWS) {
        int col = 0;
        char *token = strtok(line, ",\r\n");
        while (token && col < MAP_MAX_COLS) {
            map->tiles[row][col] = atoi(token);
            col++;
            token = strtok(NULL, ",\r\n");
        }
        if (col > map->cols) map->cols = col;
        row++;
    }
    map->rows = row;
    fclose(f);

    SDL_Log("map_load: loaded %d×%d tiles from '%s'", map->cols, map->rows, csv_path);
    return 1;
}

/* ── map_render ─────────────────────────────────────────────────────────── */
void map_render(const Map *map, SDL_Renderer *renderer,
                float cam_x, float cam_y,
                int viewport_w, int viewport_h)
{
    if (!map || !renderer || !map->tileset) return;

    /* Only draw tiles that are actually on screen (culling). */
    int first_col = (int)(cam_x / TILE_SIZE);
    int first_row = (int)(cam_y / TILE_SIZE);
    int last_col  = (int)((cam_x + viewport_w) / TILE_SIZE) + 1;
    int last_row  = (int)((cam_y + viewport_h) / TILE_SIZE) + 1;

    if (first_col < 0)          first_col = 0;
    if (first_row < 0)          first_row = 0;
    if (last_col  > map->cols)  last_col  = map->cols;
    if (last_row  > map->rows)  last_row  = map->rows;

    for (int r = first_row; r < last_row; r++) {
        for (int c = first_col; c < last_col; c++) {
            SDL_FRect src = tile_src(map, map->tiles[r][c]);

            SDL_FRect dst = {
                .x = (float)(c * TILE_SIZE) - cam_x,
                .y = (float)(r * TILE_SIZE) - cam_y,
                .w = (float)TILE_SIZE,
                .h = (float)TILE_SIZE
            };

            SDL_RenderTexture(renderer, map->tileset, &src, &dst);
        }
    }
}

/* ── map_free ───────────────────────────────────────────────────────────── */
void map_free(Map *map)
{
    if (!map) return;
    if (map->tileset) {
        SDL_DestroyTexture(map->tileset);
        map->tileset = NULL;
    }
}

/* ── map_is_wall ────────────────────────────────────────────────────────── */
int map_is_wall(const Map *map, float wx, float wy)
{
    if (!map) return 1;
    int col = (int)(wx / TILE_SIZE);
    int row = (int)(wy / TILE_SIZE);
    if (col < 0 || col >= map->cols || row < 0 || row >= map->rows) return 1;
    return (map->tiles[row][col] == 0); /* 0 = wall */
}
