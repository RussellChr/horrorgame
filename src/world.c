#include "world.h"
#include "map.h"
#include "utils.h"
#include "render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

World *world_create(void)
{
    World *w = calloc(1, sizeof(World));
    return w;
}

void world_destroy(World *world)
{
    free(world);
}

/* ── Lookup ────────────────────────────────────────────────────────────── */

Location *world_get_location(World *world, int id)
{
    if (!world) return NULL;
    for (int i = 0; i < world->location_count; i++)
        if (world->locations[i].id == id)
            return &world->locations[i];
    return NULL;
}

Location *world_get_location_by_name(World *world, const char *name)
{
    if (!world || !name) return NULL;
    for (int i = 0; i < world->location_count; i++)
        if (utils_strncasecmp(world->locations[i].name, name,
                              LOCATION_NAME_MAX) == 0)
            return &world->locations[i];
    return NULL;
}

/* ── Text-mode helpers ─────────────────────────────────────────────────── */

void world_describe_location(const Location *loc)
{
    if (!loc) return;
    printf("\n══ %s ══\n", loc->name);
    if (loc->visited)
        printf("%s\n", loc->atmosphere[0] ? loc->atmosphere : loc->description);
    else
        printf("%s\n", loc->description);

    printf("\nExits: ");
    for (int i = 0; i < loc->exit_count; i++)
        printf("[%s%s] ", loc->exits[i].direction,
               loc->exits[i].locked ? " (locked)" : "");
    if (!loc->exit_count) printf("none");
    printf("\n");
}

int world_move(World *world, int current_id,
               const char *direction, int *new_id)
{
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

void world_add_exit(Location *loc, const char *direction,
                    int target_id, int locked, int required_item_id)
{
    if (!loc || loc->exit_count >= MAX_EXITS) return;
    Exit *e = &loc->exits[loc->exit_count++];
    strncpy(e->direction, direction, sizeof(e->direction) - 1);
    e->target_id        = target_id;
    e->locked           = locked;
    e->required_item_id = required_item_id;
}

/* ── File loading ──────────────────────────────────────────────────────── */

int world_load_locations(World *world, const char *filepath)
{
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
    }

    fclose(fp);
    return 1;
}

/* ── Procedural room setup ────────────────────────────────────────────── */
/*
 * Helper macros used inside world_setup_rooms().
 */
#define ADD_DECOR(loc, X,Y,W,H, R,G,B, LBL) \
    do { \
        if ((loc)->decor_count < MAX_DECOR) { \
            Decor *_d = &(loc)->decor[(loc)->decor_count++]; \
            _d->x=(X); _d->y=(Y); _d->w=(W); _d->h=(H); \
            _d->r=(R);  _d->g=(G);  _d->b=(B); \
            strncpy(_d->label,(LBL),31); \
        } \
    } while(0)

#define ADD_COLLIDER(loc, X,Y,W,H) \
    do { \
        if ((loc)->collider_count < MAX_COLLISION_RECTS) { \
            Rect *_c = &(loc)->colliders[(loc)->collider_count++]; \
            _c->x=(float)(X); _c->y=(float)(Y); \
            _c->w=(float)(W); _c->h=(float)(H); \
        } \
    } while(0)

#define ADD_TRIGGER(loc, X,Y,W,H, TID, TX,TY) \
    do { \
        if ((loc)->trigger_count < MAX_TRIGGER_ZONES) { \
            TriggerZone *_t = &(loc)->triggers[(loc)->trigger_count++]; \
            _t->bounds.x=(float)(X); _t->bounds.y=(float)(Y); \
            _t->bounds.w=(float)(W); _t->bounds.h=(float)(H); \
            _t->target_location_id=-1; \
            _t->trigger_id=(TID); \
            _t->spawn_x=(float)(TX); _t->spawn_y=(float)(TY); \
        } \
    } while(0)

#define ADD_EXIT_TRIGGER(loc, X,Y,W,H, DEST, SX,SY) \
    do { \
        if ((loc)->trigger_count < MAX_TRIGGER_ZONES) { \
            TriggerZone *_t = &(loc)->triggers[(loc)->trigger_count++]; \
            _t->bounds.x=(float)(X); _t->bounds.y=(float)(Y); \
            _t->bounds.w=(float)(W); _t->bounds.h=(float)(H); \
            _t->target_location_id=(DEST); \
            _t->trigger_id=-1; \
            _t->spawn_x=(float)(SX); _t->spawn_y=(float)(SY); \
        } \
    } while(0)

void world_setup_rooms(World *world, SDL_Renderer *renderer)
{
    if (!world || !renderer) return;

    for (int i = 0; i < world->location_count; i++) {
        Location *loc = &world->locations[i];

        /* Default room dimensions (fallback; overridden after texture load) */
        loc->room_width  = ROOM_W;
        loc->room_height = ROOM_H;

        /* Default spawn (centre of walkable floor) */
        loc->spawn_x = (float)(loc->room_width / 2);
        loc->spawn_y = (float)(loc->room_height / 2);

        switch (loc->id) {

        /* ── 0: Entrance Hall ───────────────────────────────────────── */
            case 0: {
                loc->spawn_x = 500.0f;    // X coordinate in pixels
                loc->spawn_y = 400.0f;    // Y coordinate in pixels

                loc->background_texture = render_load_texture(
                    renderer, "assets/room/archive_room.png");
                if (loc->background_texture) {
                    float tw = 0.0f, th = 0.0f;
                    if (SDL_GetTextureSize(loc->background_texture, &tw, &th) && tw > 0 && th > 0) {
                        loc->room_width  = (int)tw/2;
                        loc->room_height = (int)th/2;
                    }
                }

                /* Load tile map and build collision from the CSV. */
                Map *m = map_load_csv("maps/archive_close.csv");
                if (m) {
                    map_build_colliders(m, loc);

                    /* Spawn in front of the door tiles (rows 10-12, cols ~271-279).
                       Hint one row below the door run so we land on floor. */
                    float sx = (float)(loc->room_width / 2);
                    float sy = (float)(loc->room_height / 2);
                    map_find_spawn(m, 13, 275, &sx, &sy,
                                   loc->room_width, loc->room_height);
                    loc->spawn_x = sx;
                    loc->spawn_y = sy;

                    map_free(m);
                } else {
                    /* Fallback spawn if CSV is missing. */
                    loc->spawn_x = (float)(loc->room_width / 2);
                    loc->spawn_y = (float)(loc->room_height / 2);
                }

                break;
            }

        case 1: {
                loc->background_texture = render_load_texture(
                    renderer, "assets/room/lab.png");
                if (loc->background_texture) {
                    float tw = 0.0f, th = 0.0f;
                    if (SDL_GetTextureSize(loc->background_texture, &tw, &th) && tw > 0 && th > 0) {
                        loc->room_width  = (int)tw;
                        loc->room_height = (int)th;
                    }
                }

                /* Load collision map and find spawn near the entrance door. */
                Map *m = map_load_csv("maps/lab.csv");
                if (m) {
                    map_build_colliders(m, loc);

                    /* Hint one row above the door tiles (rows 23-25, cols 11-14)
                       so the player spawns just in front of the door. */
                    float sx = (float)(loc->room_width  / 2);
                    float sy = (float)(loc->room_height / 2);
                    map_find_spawn(m, m->rows - 5, 12, &sx, &sy,
                                   loc->room_width, loc->room_height);
                    loc->spawn_x = sx;
                    loc->spawn_y = sy;

                    map_free(m);
                } else {
                    loc->spawn_x = (float)(loc->room_width  / 2);
                    loc->spawn_y = (float)(loc->room_height / 2);
                }

                break;
            }

        /* ── 2: Hallway ─────────────────────────────────────────────────── */
        case 2: {
                loc->background_texture = render_load_texture(
                    renderer, "assets/room/hallway.png");
                if (loc->background_texture) {
                    float tw = 0.0f, th = 0.0f;
                    if (SDL_GetTextureSize(loc->background_texture, &tw, &th) && tw > 0 && th > 0) {
                        loc->room_width  = (int)tw;
                        loc->room_height = (int)th;
                    }
                }

                /* Load collision map and find a spawn point in front of the
                   tile-5 door (rows 19-21, cols 46-48 in hallway.csv).
                   Hint one row above the door so the player stands just
                   outside it when returning from the archive room. */
                Map *m = map_load_csv("maps/hallway.csv");
                if (m) {
                    map_build_colliders(m, loc);

                    float sx = (float)(loc->room_width  / 2);
                    float sy = (float)(loc->room_height / 2);
                    map_find_spawn(m, 18, 47, &sx, &sy,
                                   loc->room_width, loc->room_height);
                    loc->spawn_x = sx;
                    loc->spawn_y = sy;

                    /* Locker interactive triggers (tile 7 in hallway.csv).
                     * Tile size: 1920/60 = 32 px wide, 960/30 = 32 px tall.
                     * Group 1: rows 14-15, cols 41-44  → world (1312,448) 128×64
                     * Group 2: rows 18-20, cols 35-39  → world (1120,576) 160×96 */
                    ADD_TRIGGER(loc, 1312, 448, 128, 64, 60, 0, 0);
                    ADD_TRIGGER(loc, 1120, 576, 160, 96, 60, 0, 0);

                    /* Tile 8: interactable (nothing here), trigger 80.
                     * Tile 9: gas mask pickup, trigger 81.
                     * Tile 10: flashlight pickup, trigger 82. */
                    map_build_interactive_triggers_for_tile(m, loc, 8,  80, 0.0f, 0.0f);
                    map_build_interactive_triggers_for_tile(m, loc, 9,  81, 0.0f, 0.0f);
                    map_build_interactive_triggers_for_tile(m, loc, 10, 82, 0.0f, 0.0f);

                    map_free(m);
                } else {
                    loc->spawn_x = (float)(loc->room_width  / 2);
                    loc->spawn_y = (float)(loc->room_height / 2);
                }

                break;
            }

        /* ── 3: Hibernation ─────────────────────────────────────────────── */
        case 3: {
                loc->background_texture = render_load_texture(
                    renderer, "assets/room/hibernation.png");
                if (loc->background_texture) {
                    float tw = 0.0f, th = 0.0f;
                    if (SDL_GetTextureSize(loc->background_texture, &tw, &th) && tw > 0 && th > 0) {
                        loc->room_width  = (int)tw;
                        loc->room_height = (int)th;
                    }
                }

                Map *m = map_load_csv("maps/hibernation.csv");
                if (m) {
                    map_build_colliders(m, loc);

                    /* Spawn just below the tile-5 connector (rows 4-7, cols 33-36).
                       Hint one row below the block so the player stands in front of
                       the door when arriving from the hallway. */
                    float sx = (float)(loc->room_width  / 2);
                    float sy = (float)(loc->room_height / 2);
                    map_find_spawn(m, 9, 34, &sx, &sy,
                                   loc->room_width, loc->room_height);
                    loc->spawn_x = sx;
                    loc->spawn_y = sy;

                    /* Interactive triggers for tile 1 (zonk), 2 (powercell),
                       3 (powercell slot), 4 (flavor description).
                       Trigger IDs 71–74 are handled in game.c handle_interaction. */
                    map_build_interactive_triggers_for_tile(m, loc, 1, 71, 0.0f, 0.0f);
                    map_build_interactive_triggers_for_tile(m, loc, 2, 72, 0.0f, 0.0f);
                    map_build_interactive_triggers_for_tile(m, loc, 3, 73, 0.0f, 0.0f);
                    map_build_interactive_triggers_for_tile(m, loc, 4, 74, 0.0f, 0.0f);
                    /* Tile-5 door: physical collision barrier until the powercell is
                       placed.  Build colliders for tile-5, recording the range so
                       game.c can remove them when the door opens.
                       Also add interactive trigger 75 so the locked message is shown
                       while the door is still blocked.
                       Spawn coords are filled in below after hw1x/hw1y are known. */
                    loc->door_collider_start = loc->collider_count;
                    map_build_colliders_for_tile(m, loc, 5);
                    loc->door_collider_count = loc->collider_count - loc->door_collider_start;
                    map_build_interactive_triggers_for_tile(m, loc, 5, 75, 0.0f, 0.0f);

                    map_free(m);
                } else {
                    loc->spawn_x = (float)(loc->room_width  / 2);
                    loc->spawn_y = (float)(loc->room_height / 2);
                }

                break;
            }

        /* ── 4: Power ───────────────────────────────────────────────────── */
        case 4: {
                loc->background_texture = render_load_texture(
                    renderer, "assets/room/power.png");
                if (loc->background_texture) {
                    float tw = 0.0f, th = 0.0f;
                    if (SDL_GetTextureSize(loc->background_texture, &tw, &th) && tw > 0 && th > 0) {
                        loc->room_width  = (int)tw;
                        loc->room_height = (int)th;
                    }
                }

                Map *m = map_load_csv("maps/power.csv");
                if (m) {
                    map_build_colliders(m, loc);

                    /* Spawn to the left of the tile-5 connector (rows 19-25, cols 53-55).
                       Hint into the floor area adjacent to the connector so the player
                       stands in front of the door when arriving from the hallway. */
                    float sx = (float)(loc->room_width  / 2);
                    float sy = (float)(loc->room_height / 2);
                    map_find_spawn(m, 21, 50, &sx, &sy,
                                   loc->room_width, loc->room_height);
                    loc->spawn_x = sx;
                    loc->spawn_y = sy;

                    map_free(m);
                } else {
                    loc->spawn_x = (float)(loc->room_width  / 2);
                    loc->spawn_y = (float)(loc->room_height / 2);
                }

                break;
            }

        /* ── 5: Security ────────────────────────────────────────────────── */
        case 5: {
                loc->background_texture = render_load_texture(
                    renderer, "assets/room/monitoring_room.png");
                if (loc->background_texture) {
                    float tw = 0.0f, th = 0.0f;
                    if (SDL_GetTextureSize(loc->background_texture, &tw, &th) && tw > 0 && th > 0) {
                        loc->room_width  = (int)tw;
                        loc->room_height = (int)th;
                    }
                }

                Map *m = map_load_csv("maps/security.csv");
                if (m) {
                    map_build_colliders(m, loc);

                    /* Spawn just above the tile-3 connector (rows 20-21, cols 28-34).
                       Hint one row above the block so the player stands in
                       front of the connector when arriving from the hallway. */
                    float sx = (float)(loc->room_width  / 2);
                    float sy = (float)(loc->room_height / 2);
                    map_find_spawn(m, 19, 31, &sx, &sy,
                                   loc->room_width, loc->room_height);
                    loc->spawn_x = sx;
                    loc->spawn_y = sy;

                    map_free(m);
                } else {
                    loc->spawn_x = (float)(loc->room_width  / 2);
                    loc->spawn_y = (float)(loc->room_height / 2);
                }

                break;
            }

        default:
            break;
        }
    }

    /* ── Post-setup: link door triggers between all rooms and the Hallway ── */
    /*
     * Tile mapping (hallway.csv, 0-indexed rows / cols):
     *   tile 5 (archive):     rows 19-21, cols 45-47  → hint (18, 46)
     *   tile 4 (lab):         rows 13-15, cols 29-30  → hint (16, 29)
     *   tile 1 (hibernation): rows 25-27, cols 14-16  → hint (24, 15)
     *   tile 2 (power):       rows 14-16, cols  3- 4  → hint (19,  6)
     *   tile 3 (security):    rows  6- 8, cols 14-15  → hint ( 9, 15)
     *
     * In each sub-room the connector back to the hallway:
     *   Archive     → tile-1 (MAP_TILE_DOOR) in archive_close.csv
     *   Lab         → tile-1 (MAP_TILE_DOOR) in lab.csv
     *   Hibernation → tile-5 in hibernation.csv
     *   Power       → tile-5 in power.csv
     *   Security    → tile-3 in security.csv  (rows 20-21, cols 28-34)
     */
    {
        Location *loc0 = world_get_location(world, 0); /* Archive     */
        Location *loc1 = world_get_location(world, 1); /* Lab         */
        Location *loc2 = world_get_location(world, 2); /* Hallway     */
        Location *loc3 = world_get_location(world, 3); /* Hibernation */
        Location *loc4 = world_get_location(world, 4); /* Power       */
        Location *loc5 = world_get_location(world, 5); /* Security    */

        /* Compute return-spawn positions in the hallway near each door tile.
         * Hints are placed safely outside the tile block so the player lands
         * on walkable floor without overlapping a door trigger on arrival.
         *   tile 1 hint: (24, 15) – one row above the hibernation door
         *   tile 2 hint: (19,  6) – one row below the power door block
         *                           (rows 15-18 are tile-2; row 19 col 6 is floor
         *                            and avoids the feet-rect overlap that caused
         *                            the Power-room teleport loop)
         *   tile 3 hint: ( 9, 15) – one row below the security door
         *   tile 4 hint: (16, 29) – one row below the lab door        */
        float hw5x = 0.0f, hw5y = 0.0f;
        float hw1x = 0.0f, hw1y = 0.0f;
        float hw2x = 0.0f, hw2y = 0.0f;
        float hw3x = 0.0f, hw3y = 0.0f;
        float hw4x = 0.0f, hw4y = 0.0f;

        if (loc2) {
            hw5x = hw1x = hw2x = hw3x = hw4x = loc2->spawn_x;
            hw5y = hw1y = hw2y = hw3y = hw4y = loc2->spawn_y;

            Map *mhw = map_load_csv("maps/hallway.csv");
            if (mhw) {
                /* tile 5 hint: (18, 46) – one row above the archive door */
                map_find_spawn(mhw, 18, 46, &hw5x, &hw5y,
                               loc2->room_width, loc2->room_height);
                map_find_spawn(mhw, 24, 15, &hw1x, &hw1y,
                               loc2->room_width, loc2->room_height);
                map_find_spawn(mhw, 19,  6, &hw2x, &hw2y,
                               loc2->room_width, loc2->room_height);
                map_find_spawn(mhw,  9, 15, &hw3x, &hw3y,
                               loc2->room_width, loc2->room_height);
                map_find_spawn(mhw, 16, 29, &hw4x, &hw4y,
                               loc2->room_width, loc2->room_height);

                /* Hallway → each room (all tile values in a single CSV load). */
                if (loc0) map_build_door_triggers_for_tile(mhw, loc2, 5, 0,
                                                           loc0->spawn_x, loc0->spawn_y);
                if (loc1) map_build_door_triggers_for_tile(mhw, loc2, 4, 1,
                                                           loc1->spawn_x, loc1->spawn_y);
                if (loc3) map_build_door_triggers_for_tile(mhw, loc2, 1, 3,
                                                           loc3->spawn_x, loc3->spawn_y);
                if (loc4) map_build_door_triggers_for_tile(mhw, loc2, 2, 4,
                                                           loc4->spawn_x, loc4->spawn_y);
                if (loc5) map_build_door_triggers_for_tile(mhw, loc2, 3, 5,
                                                           loc5->spawn_x, loc5->spawn_y);
                map_free(mhw);
            }
        }

        /* Archive tile-1 → Hallway (spawn near the tile-5 door). */
        if (loc0 && loc2) {
            Map *m = map_load_csv("maps/archive_close.csv");
            if (m) {
                map_build_door_triggers(m, loc0, 2, hw5x, hw5y);
                map_free(m);
            }
        }

        /* Lab tile-1 → Hallway (spawn near the tile-4 door). */
        if (loc1 && loc2) {
            Map *m = map_load_csv("maps/lab.csv");
            if (m) {
                map_build_door_triggers(m, loc1, 2, hw4x, hw4y);
                map_free(m);
            }
        }

        /* Hibernation tile-5 → Hallway (spawn near the tile-1 door).
           Tile-5 zones were already created as interactive trigger 75 during
           room setup; patch their spawn coords now that hw1x/hw1y are known. */
        if (loc3 && loc2) {
            for (int i = 0; i < loc3->trigger_count; i++) {
                if (loc3->triggers[i].trigger_id == 75) {
                    loc3->triggers[i].spawn_x = hw1x;
                    loc3->triggers[i].spawn_y = hw1y;
                }
            }
        }

        /* Power tile-5 → Hallway (spawn near the tile-2 door). */
        if (loc4 && loc2) {
            Map *m = map_load_csv("maps/power.csv");
            if (m) {
                map_build_door_triggers_for_tile(m, loc4, 5, 2, hw2x, hw2y);
                map_free(m);
            }
        }

        /* Security tile-3 → Hallway (spawn near the tile-3 door).
           security.csv has tile-3 cells at rows 20-21, cols 28-34. */
        if (loc5 && loc2) {
            Map *m = map_load_csv("maps/security.csv");
            if (m) {
                map_build_door_triggers_for_tile(m, loc5, 3, 2, hw3x, hw3y);
                map_free(m);
            }
        }
    }
}

/* ── Room rendering ────────────────────────────────────────────────────── */
void world_render_room(const Location *loc, SDL_Renderer *renderer,
                       const Camera *cam)
{
    if (!loc || !renderer || !cam) return;

    int cx = (int)cam->x;
    int cy = (int)cam->y;

    /* Draw background texture if available, otherwise use colored rectangles */
    if (loc->background_texture) {
        render_texture(renderer, loc->background_texture,
                      -cx, -cy, loc->room_width, loc->room_height);
    } else {
        /* ── Ceiling ── */
        render_filled_rect(renderer,
            -cx, 0,
            loc->room_width, FLOOR_Y - 40,
            loc->wall_r, loc->wall_g, loc->wall_b, 255);

        /* ── Ceiling strip ── */
        render_filled_rect(renderer,
            -cx, 0,
            loc->room_width, 40,
            loc->ceil_r, loc->ceil_g, loc->ceil_b, 255);

        /* ── Wall bottom strip ── */
        render_filled_rect(renderer,
            -cx, FLOOR_Y - 40,
            loc->room_width, 40,
            (Uint8)(loc->wall_r/2), (Uint8)(loc->wall_g/2), (Uint8)(loc->wall_b/2),
            255);

        /* ── Floor ── */
        render_filled_rect(renderer,
            -cx, FLOOR_Y,
            loc->room_width, loc->room_height - FLOOR_Y,
            loc->floor_r, loc->floor_g, loc->floor_b, 255);

        /* Floor planks / detail lines */
        for (int fx = 0; fx < loc->room_width; fx += 120) {
            int lx = fx - cx;
            SDL_SetRenderDrawColor(renderer,
                (Uint8)(loc->floor_r > 15 ? loc->floor_r - 15 : 0),
                (Uint8)(loc->floor_g > 15 ? loc->floor_g - 15 : 0),
                (Uint8)(loc->floor_b > 15 ? loc->floor_b - 15 : 0),
                255);
            SDL_RenderLine(renderer, (float)lx, (float)FLOOR_Y,
                           (float)lx, (float)loc->room_height);
        }
    }

    /* ── Decorations ── */
    for (int i = 0; i < loc->decor_count; i++) {
        const Decor *d = &loc->decor[i];
        render_filled_rect(renderer,
            d->x - cx, d->y - cy,
            d->w, d->h,
            d->r, d->g, d->b, 255);

        /* Outline to give depth */
        render_rect_outline(renderer,
            d->x - cx, d->y - cy,
            d->w, d->h,
            (Uint8)(d->r > 30 ? d->r - 30 : 0),
            (Uint8)(d->g > 30 ? d->g - 30 : 0),
            (Uint8)(d->b > 30 ? d->b - 30 : 0),
            200);
    }
    /* ── Collision boxes (debug visualization) ── */
    for (int i = 0; i < loc->collider_count; i++) {
        const Rect *col = &loc->colliders[i];
        render_rect_outline(renderer,
            (int)(col->x - cx), (int)(col->y - cy),
            (int)col->w, (int)col->h,
            255, 0, 0, 255);  /* Red outline */
    }
    /* ── Danger zone vignette ── */
    if (loc->is_danger_zone) {
        /* Red border on all four edges */
        for (int t = 0; t < 4; t++) {
            Uint8 alpha = (Uint8)(40 + t * 15);
            render_filled_rect(renderer,  0,       0, WINDOW_W, 4+t*2,
                               120,0,0,alpha);
            render_filled_rect(renderer,  0, WINDOW_H-4-t*2, WINDOW_W, 4+t*2,
                               120,0,0,alpha);
            render_filled_rect(renderer,  0,       0, 4+t*2, WINDOW_H,
                               120,0,0,alpha);
            render_filled_rect(renderer, WINDOW_W-4-t*2,  0, 4+t*2, WINDOW_H,
                               120,0,0,alpha);
        }
    }

    /* ── Location name (top centre) ── */
    render_text_centered(renderer, loc->name,
                         WINDOW_W / 2, 10, 1, 180,160,140);
}
