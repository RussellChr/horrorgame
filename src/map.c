#include "map.h"
#include "collision.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

Map *map_load_csv(const char *filepath)
{
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        fprintf(stderr, "map: cannot open '%s'\n", filepath);
        return NULL;
    }

    /* First pass: count rows and columns. */
    int rows = 0, cols = 0;
    char line[8192];
    while (fgets(line, sizeof(line), fp)) {
        char *l = line;
        while (*l == ' ' || *l == '\t') l++;
        if (*l == '\0' || *l == '\r' || *l == '\n') continue;

        if (rows == 0) {
            /* Count commas + 1 for column count. */
            int c = 1;
            for (int i = 0; line[i]; i++)
                if (line[i] == ',') c++;
            cols = c;
        }
        rows++;
    }

    if (rows == 0 || cols == 0) {
        fclose(fp);
        fprintf(stderr, "map: empty or malformed CSV '%s'\n", filepath);
        return NULL;
    }

    Map *map = malloc(sizeof(Map));
    if (!map) { fclose(fp); return NULL; }

    map->rows  = rows;
    map->cols  = cols;
    map->cells = malloc(sizeof(int) * (size_t)(rows * cols));
    if (!map->cells) {
        free(map);
        fclose(fp);
        return NULL;
    }

    /* Second pass: fill the grid. */
    rewind(fp);
    int r = 0;
    while (fgets(line, sizeof(line), fp)) {
        char *l = line;
        while (*l == ' ' || *l == '\t') l++;
        if (*l == '\0' || *l == '\r' || *l == '\n') continue;

        char *tok = l;
        for (int c = 0; c < cols; c++) {
            map->cells[r * cols + c] = (int)strtol(tok, &tok, 10);
            if (*tok == ',') tok++;   /* skip comma */
        }
        r++;
        if (r >= rows) break;
    }

    fclose(fp);
    return map;
}

void map_free(Map *map)
{
    if (!map) return;
    free(map->cells);
    free(map);
}

/* ── Collider building ─────────────────────────────────────────────────── */

int map_build_colliders(const Map *map, Location *loc)
{
    if (!loc) return -1;
    if (!map || !map->cells) return 0;

    int added = 0;
    float tile_w = (float)loc->room_width  / (float)map->cols;
    float tile_h = (float)loc->room_height / (float)map->rows;

    for (int r = 0; r < map->rows; r++) {
        int c = 0;
        while (c < map->cols) {
            if (map->cells[r * map->cols + c] == MAP_TILE_WALL) {
                /* Start of a wall run – extend right as far as possible. */
                int run_start = c;
                while (c < map->cols &&
                       map->cells[r * map->cols + c] == MAP_TILE_WALL)
                    c++;
                /* [run_start, c) are all wall tiles on row r. */
                if (loc->collider_count < MAX_COLLISION_RECTS) {
                    Rect *rect = &loc->colliders[loc->collider_count++];
                    rect->x = (float)run_start * tile_w;
                    rect->y = (float)r          * tile_h;
                    rect->w = (float)(c - run_start) * tile_w;
                    rect->h = tile_h;
                    added++;
                }
            } else {
                c++;
            }
        }
    }

    return added;
}

/* ── Door-trigger building ─────────────────────────────────────────────── */

int map_build_door_triggers_for_tile(const Map *map, Location *loc,
                                     int tile,
                                     int dest_id,
                                     float dest_spawn_x, float dest_spawn_y)
{
    if (!map || !map->cells || !loc) return 0;

    int added = 0;
    float tile_w = (float)loc->room_width  / (float)map->cols;
    float tile_h = (float)loc->room_height / (float)map->rows;

    for (int r = 0; r < map->rows; r++) {
        int c = 0;
        while (c < map->cols) {
            if (map->cells[r * map->cols + c] == tile) {
                /* Start of a door run – extend right as far as possible. */
                int run_start = c;
                while (c < map->cols &&
                       map->cells[r * map->cols + c] == tile)
                    c++;
                if (loc->trigger_count < MAX_TRIGGER_ZONES) {
                    TriggerZone *tz = &loc->triggers[loc->trigger_count++];
                    tz->bounds.x           = (float)run_start * tile_w;
                    tz->bounds.y           = (float)r          * tile_h;
                    tz->bounds.w           = (float)(c - run_start) * tile_w;
                    tz->bounds.h           = tile_h;
                    tz->target_location_id = dest_id;
                    tz->trigger_id         = -1;
                    tz->spawn_x            = dest_spawn_x;
                    tz->spawn_y            = dest_spawn_y;
                    added++;
                }
            } else {
                c++;
            }
        }
    }

    return added;
}

int map_build_door_triggers(const Map *map, Location *loc,
                            int dest_id,
                            float dest_spawn_x, float dest_spawn_y)
{
    return map_build_door_triggers_for_tile(map, loc, MAP_TILE_DOOR,
                                            dest_id, dest_spawn_x, dest_spawn_y);
}

int map_build_interactive_triggers_for_tile(const Map *map, Location *loc,
                                             int tile, int trigger_id,
                                             float spawn_x, float spawn_y)
{
    if (!map || !map->cells || !loc) return 0;

    int added = 0;
    float tile_w = (float)loc->room_width  / (float)map->cols;
    float tile_h = (float)loc->room_height / (float)map->rows;

    for (int r = 0; r < map->rows; r++) {
        int c = 0;
        while (c < map->cols) {
            if (map->cells[r * map->cols + c] == tile) {
                int run_start = c;
                while (c < map->cols &&
                       map->cells[r * map->cols + c] == tile)
                    c++;
                if (loc->trigger_count < MAX_TRIGGER_ZONES) {
                    TriggerZone *tz = &loc->triggers[loc->trigger_count++];
                    tz->bounds.x           = (float)run_start * tile_w;
                    tz->bounds.y           = (float)r          * tile_h;
                    tz->bounds.w           = (float)(c - run_start) * tile_w;
                    tz->bounds.h           = tile_h;
                    tz->target_location_id = -1;
                    tz->trigger_id         = trigger_id;
                    tz->spawn_x            = spawn_x;
                    tz->spawn_y            = spawn_y;
                    added++;
                }
            } else {
                c++;
            }
        }
    }

    return added;
}

/* ── Spawn search ─────────────────────────────────────────────────────── */

int map_find_spawn(const Map *map, int hint_row, int hint_col,
                   float *out_x, float *out_y, int room_w, int room_h)
{
    if (!map || !map->cells || !out_x || !out_y) return 0;

    float tile_w = (float)room_w / (float)map->cols;
    float tile_h = (float)room_h / (float)map->rows;

    /* Search outward from the hint position. */
    for (int radius = 0; radius < map->rows; radius++) {
        for (int dr = -radius; dr <= radius; dr++) {
            for (int dc = -radius; dc <= radius; dc++) {
                /* Only visit the outer ring at each radius. */
                if (abs(dr) != radius && abs(dc) != radius) continue;

                int rr = hint_row + dr;
                int cc = hint_col + dc;
                if (rr < 0 || rr >= map->rows) continue;
                if (cc < 0 || cc >= map->cols) continue;

                if (map->cells[rr * map->cols + cc] == MAP_TILE_FLOOR) {
                    /* Place the player's feet (y = centre-bottom) at the
                       bottom edge of the tile so they land on the floor. */
                    *out_x = ((float)cc + 0.5f) * tile_w;
                    *out_y = ((float)rr + 1.0f) * tile_h;
                    return 1;
                }
            }
        }
    }

    return 0;
}
