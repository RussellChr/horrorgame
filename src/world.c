#include "world.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

World *world_create(void) {
    World *w = calloc(1, sizeof(World));
    return w;
}

void world_destroy(World *world) {
    free(world);
}

/* ── Lookup ────────────────────────────────────────────────────────────── */

Location *world_get_location(World *world, int id) {
    if (!world) return NULL;
    for (int i = 0; i < world->location_count; i++) {
        if (world->locations[i].id == id)
            return &world->locations[i];
    }
    return NULL;
}

Location *world_get_location_by_name(World *world, const char *name) {
    if (!world || !name) return NULL;
    for (int i = 0; i < world->location_count; i++) {
        if (utils_strncasecmp(world->locations[i].name, name,
                              LOCATION_NAME_MAX) == 0)
            return &world->locations[i];
    }
    return NULL;
}

/* ── Description ───────────────────────────────────────────────────────── */

void world_describe_location(const Location *loc) {
    if (!loc) return;
    printf("\n══ %s ══\n", loc->name);
    if (loc->visited) {
        /* Show shorter atmospheric text on revisit */
        if (loc->atmosphere[0])
            printf("%s\n", loc->atmosphere);
        else
            printf("%s\n", loc->description);
    } else {
        printf("%s\n", loc->description);
    }
    /* List available exits */
    printf("\nExits: ");
    if (loc->exit_count == 0) {
        printf("none");
    } else {
        for (int i = 0; i < loc->exit_count; i++) {
            if (loc->exits[i].locked)
                printf("[%s (locked)] ", loc->exits[i].direction);
            else
                printf("[%s] ", loc->exits[i].direction);
        }
    }
    printf("\n");
}

/* ── Movement ──────────────────────────────────────────────────────────── */

int world_move(World *world, int current_id,
               const char *direction, int *new_id) {
    Location *loc = world_get_location(world, current_id);
    if (!loc) return 0;

    for (int i = 0; i < loc->exit_count; i++) {
        if (utils_strncasecmp(loc->exits[i].direction, direction,
                              sizeof(loc->exits[i].direction)) == 0) {
            if (loc->exits[i].locked) {
                printf("That way is locked.\n");
                return 0;
            }
            *new_id = loc->exits[i].target_id;
            return 1;
        }
    }
    printf("You can't go that way.\n");
    return 0;
}

/* ── Exit helpers ──────────────────────────────────────────────────────── */

void world_add_exit(Location *loc, const char *direction,
                    int target_id, int locked, int required_item_id) {
    if (!loc || loc->exit_count >= MAX_EXITS) return;
    Exit *e = &loc->exits[loc->exit_count++];
    strncpy(e->direction, direction, sizeof(e->direction) - 1);
    e->target_id        = target_id;
    e->locked           = locked;
    e->required_item_id = required_item_id;
}

/* ── File loading ──────────────────────────────────────────────────────── */

/*
 * Simple line-based parser for assets/locations.txt
 *
 * Format:
 *   LOCATION <id> <name>
 *   DESC <description text>
 *   ATMO <atmosphere text>
 *   EXIT <direction> <target_id> [locked] [required_item_id]
 *   ITEM <item_id>
 *   DANGER
 *   END
 */
int world_load_locations(World *world, const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        fprintf(stderr, "world: cannot open '%s'\n", filepath);
        return 0;
    }

    char line[1024];
    Location *cur = NULL;

    while (fgets(line, sizeof(line), fp)) {
        char *l = utils_strtrim(line);
        if (!l || l[0] == '#' || l[0] == '\0') continue;

        if (strncmp(l, "LOCATION ", 9) == 0) {
            if (world->location_count >= MAX_LOCATIONS) break;
            cur = &world->locations[world->location_count++];
            memset(cur, 0, sizeof(Location));
            sscanf(l + 9, "%d", &cur->id);
            /* name is everything after the id token */
            const char *p = l + 9;
            while (*p && *p != ' ') p++;
            while (*p == ' ') p++;
            strncpy(cur->name, p, LOCATION_NAME_MAX - 1);
        } else if (strncmp(l, "DESC ", 5) == 0 && cur) {
            strncpy(cur->description, l + 5, LOCATION_DESC_MAX - 1);
        } else if (strncmp(l, "ATMO ", 5) == 0 && cur) {
            strncpy(cur->atmosphere, l + 5, LOCATION_DESC_MAX - 1);
        } else if (strncmp(l, "EXIT ", 5) == 0 && cur) {
            char dir[16] = {0};
            int target = 0, locked = 0, req = 0;
            sscanf(l + 5, "%15s %d %d %d", dir, &target, &locked, &req);
            world_add_exit(cur, dir, target, locked, req);
        } else if (strncmp(l, "ITEM ", 5) == 0 && cur) {
            sscanf(l + 5, "%d", &cur->item_id);
        } else if (strncmp(l, "DANGER", 6) == 0 && cur) {
            cur->is_danger_zone = 1;
        }
        /* END is a no-op; just resets cur conceptually */
    }

    fclose(fp);
    return 1;
}
