#include "tilemap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Enough for up to 300 tile IDs × ~6 chars each ("10475,"), plus headroom. */
#define TILEMAP_LINE_BUF 8192

int tilemap_load_colliders(const char *filepath, Location *loc, int tile_size)
{
    if (!filepath || !loc || tile_size <= 0) return -1;

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        fprintf(stderr, "tilemap: cannot open '%s'\n", filepath);
        return -1;
    }

    char line[TILEMAP_LINE_BUF];
    int  added = 0;
    int  row   = 0;

    while (fgets(line, sizeof(line), fp)) {
        /* Strip trailing whitespace / newlines. */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' ||
                           line[len - 1] == ' '))
            line[--len] = '\0';

        if (len == 0) continue;

        /*
         * Parse comma-separated tile IDs and perform horizontal run-length
         * merging: consecutive solid (ID > 0) tiles in the same row are
         * combined into a single Rect.
         */
        int   col       = 0;
        int   run_start = -1; /* column where the current solid run began */
        char *p         = line;

        while (*p != '\0') {
            char *end;
            long  tile_id = strtol(p, &end, 10);
            if (end == p) break; /* nothing parsed – malformed token */
            p = end;
            if (*p == ',') p++;

            if (tile_id > 0) {
                /* Solid tile: start a run if not already in one. */
                if (run_start < 0) run_start = col;
            } else {
                /* Empty tile: flush any open run. */
                if (run_start >= 0) {
                    if (loc->collider_count < MAX_COLLISION_RECTS) {
                        Rect *r = &loc->colliders[loc->collider_count++];
                        r->x = (float)(run_start * tile_size);
                        r->y = (float)(row       * tile_size);
                        r->w = (float)((col - run_start) * tile_size);
                        r->h = (float)(tile_size);
                        added++;
                    } else {
                        fprintf(stderr,
                            "tilemap: collider limit (%d) reached; "
                            "some solid tiles will have no collision\n",
                            MAX_COLLISION_RECTS);
                    }
                    run_start = -1;
                }
            }
            col++;
        }

        if (run_start >= 0) {
            if (loc->collider_count < MAX_COLLISION_RECTS) {
                Rect *r = &loc->colliders[loc->collider_count++];
                r->x = (float)(run_start * tile_size);
                r->y = (float)(row       * tile_size);
                r->w = (float)((col - run_start) * tile_size);
                r->h = (float)(tile_size);
                added++;
            } else {
                fprintf(stderr,
                    "tilemap: collider limit (%d) reached; "
                    "some solid tiles will have no collision\n",
                    MAX_COLLISION_RECTS);
            }
        }

        row++;
    }

    fclose(fp);
    return added;
}
