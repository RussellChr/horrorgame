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
                Map *m = map_load_csv("maps/logic archive tutup_logic.csv");
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
                Map *m = map_load_csv("maps/logic kimia_logic.csv");
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
        case 2: {
                loc->background_texture = render_load_texture(
                    renderer, "assets/room/monitoring_room.png");
                if (loc->background_texture) {
                    float tw = 0.0f, th = 0.0f;
                    if (SDL_GetTextureSize(loc->background_texture, &tw, &th) && tw > 0 && th > 0) {
                        loc->room_width  = (int)tw;
                        loc->room_height = (int)th;
                    }
                }
                /* Load collision map and find spawn near the entrance door. */
                Map *m = map_load_csv("maps/logic kimia_logic.csv");
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
        default:
            break;
        }
    }

    /* ── Post-setup: link door triggers between rooms ──────────────────── */
    /* Room 0's entrance door (tile 1 in archive tutup_logic.csv) leads to
       room 1 (kimia).  We reload the CSV here so the spawn coordinates from
       room 1's setup can be used as the destination spawn position. */
    {
        Location *loc0 = world_get_location(world, 0);
        Location *loc1 = world_get_location(world, 1);
        if (loc0 && loc1) {
            Map *m = map_load_csv("maps/logic archive tutup_logic.csv");
            if (m) {
                map_build_door_triggers(m, loc0, 1,
                                        loc1->spawn_x, loc1->spawn_y);
                map_free(m);
            }

            /* Room 1's door tiles (tile 1 in logic kimia_logic.csv) lead back
               to room 0, spawning the player in front of room 0's door. */
            Map *m2 = map_load_csv("maps/logic kimia_logic.csv");
            if (m2) {
                map_build_door_triggers(m2, loc1, 0,
                                        loc0->spawn_x, loc0->spawn_y);
                map_free(m2);
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
