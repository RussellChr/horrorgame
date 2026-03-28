#include "tilemap.h"
#include "render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

Tilemap *tilemap_create(int cols, int rows)
{
    Tilemap *map = calloc(1, sizeof(Tilemap));
    if (!map) return NULL;

    map->cols      = cols;
    map->rows      = rows;
    map->tile_size = TILE_SIZE;

    map->tiles = malloc((size_t)(cols * rows) * sizeof(int));
    if (!map->tiles) {
        free(map);
        return NULL;
    }

    /* Initialise every tile to -1 (floor) */
    for (int i = 0; i < cols * rows; i++)
        map->tiles[i] = -1;

    return map;
}

void tilemap_destroy(Tilemap *map)
{
    if (!map) return;
    if (map->tileset)
        render_texture_destroy(map->tileset);
    free(map->tiles);
    free(map);
}

/* ── CSV loading ───────────────────────────────────────────────────────── */

int tilemap_load_csv(Tilemap *map, const char *filepath)
{
    if (!map || !filepath) return 0;

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        fprintf(stderr, "tilemap: cannot open '%s'\n", filepath);
        return 0;
    }

    char line[4096];
    int row = 0;

    while (row < map->rows && fgets(line, sizeof(line), fp)) {
        int  col = 0;
        char *p  = line;

        while (col < map->cols && *p) {
            /* skip leading whitespace */
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
            if (!*p) break;

            char *end = p;
            int   val = (int)strtol(p, &end, 10);
            if (end == p) break; /* no digits found */
            p = end;

            tilemap_set_tile(map, col, row, val);
            col++;

            /* skip comma separator */
            if (*p == ',') p++;
        }
        row++;
    }

    fclose(fp);
    return 1;
}

/* ── Tile accessors ────────────────────────────────────────────────────── */

int tilemap_get_tile(const Tilemap *map, int col, int row)
{
    if (!map || col < 0 || col >= map->cols || row < 0 || row >= map->rows)
        return -1;
    return map->tiles[row * map->cols + col];
}

void tilemap_set_tile(Tilemap *map, int col, int row, int tile_id)
{
    if (!map || col < 0 || col >= map->cols || row < 0 || row >= map->rows)
        return;
    map->tiles[row * map->cols + col] = tile_id;
}

/* ── Rendering ─────────────────────────────────────────────────────────── */

void tilemap_render(const Tilemap *map, SDL_Renderer *renderer,
                    int camera_x, int camera_y)
{
    if (!map || !renderer) return;

    int ts         = map->tile_size;
    int viewport_w = 1280; /* WINDOW_W */
    int viewport_h = 720;  /* WINDOW_H */

    for (int row = 0; row < map->rows; row++) {
        for (int col = 0; col < map->cols; col++) {
            int screen_x = col * ts - camera_x;
            int screen_y = row * ts - camera_y;

            /* Frustum cull: skip tiles fully outside the viewport */
            if (screen_x + ts <= 0 || screen_x >= viewport_w) continue;
            if (screen_y + ts <= 0 || screen_y >= viewport_h) continue;

            int tile_id = tilemap_get_tile(map, col, row);

            if (tile_id == -1) {
                /* Floor tile – plain walkable surface */
                render_filled_rect(renderer, screen_x, screen_y, ts, ts,
                                   120, 100, 80, 255);
            } else if (tile_id >= 0 && map->tileset) {
                /* Render sub-region of tileset atlas */
                int tcols   = map->tileset_cols > 0 ? map->tileset_cols : 1;
                int src_col = tile_id % tcols;
                int src_row = tile_id / tcols;

                SDL_FRect src = { (float)(src_col * ts), (float)(src_row * ts),
                                  (float)ts, (float)ts };
                SDL_FRect dst = { (float)screen_x, (float)screen_y,
                                  (float)ts, (float)ts };
                SDL_RenderTexture(renderer, map->tileset, &src, &dst);
            } else if (tile_id == 0) {
                /* Wall tile – dark solid block (fallback, no tileset) */
                render_filled_rect(renderer, screen_x, screen_y, ts, ts,
                                   60, 55, 50, 255);
            }
        }
    }
}
