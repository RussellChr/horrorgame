#include "world.h"
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

void world_setup_rooms(World *world)
{
    if (!world) return;

    for (int i = 0; i < world->location_count; i++) {
        Location *loc = &world->locations[i];

        /* Default spawn (centre of walkable floor) */
        loc->spawn_x = (float)(ROOM_W / 2);
        loc->spawn_y = (float)(ROOM_H / 2);

        switch (loc->id) {

        /* ── 0: Entrance Hall ───────────────────────────────────────── */
            case 0: {
                int npc_x = STRANGER_NPC_X;
                int npc_y = 360;

                loc->wall_r  = 40;  loc->wall_g  = 30;  loc->wall_b  = 25;
                loc->floor_r = 90;  loc->floor_g = 70;  loc->floor_b = 50;
                loc->ceil_r  = 20;  loc->ceil_g  = 15;  loc->ceil_b  = 10;
                loc->spawn_x = (float)(ROOM_W / 2);
                loc->spawn_y = (float)(ROOM_H / 2);

                /* Portrait on the left wall */
                ADD_DECOR(loc, 160, 270, 80, 100, 180, 150, 120, "portrait");

                /* Stranger NPC - body and head */
                ADD_DECOR(loc, npc_x,     npc_y,      30,  60, 230, 200,  30, "body");
                ADD_DECOR(loc, npc_x - 7, npc_y - 70, 44,  44, 240, 210,  40, "head");

                /* Portrait interaction trigger (trigger_id=30) */
                ADD_TRIGGER(loc, 120, 240, 160, 160, 30, 0.0f, 0.0f);

                /* Stranger NPC interaction trigger (trigger_id=40) */
                ADD_TRIGGER(loc, npc_x - 80, npc_y - 100, 160, 180, 40, 0.0f, 0.0f);

                /* Exit right → Corridor (Location 1); spawn player at centre of loc 1 */
                ADD_EXIT_TRIGGER(loc, ROOM_W - 60, 200, 60, 300,
                                 1, (float)(ROOM_W / 2), (float)(ROOM_H / 2));

                /* Colliders */
                ADD_COLLIDER(loc, 0, 0, 40, ROOM_H);           /* left  */
                ADD_COLLIDER(loc, ROOM_W - 40, 0, 40, ROOM_H); /* right */
                ADD_COLLIDER(loc, 0, 0, ROOM_W, 40);           /* top   */
                ADD_COLLIDER(loc, 0, ROOM_H - 40, ROOM_W, 40); /* bottom */
                break;
            }

        /* ── 1: Corridor ─────────────────────────────────────────────── */
            case 1: {
                loc->wall_r  = 35;  loc->wall_g  = 35;  loc->wall_b  = 40;
                loc->floor_r = 70;  loc->floor_g = 65;  loc->floor_b = 60;
                loc->ceil_r  = 15;  loc->ceil_g  = 15;  loc->ceil_b  = 20;
                loc->spawn_x = (float)(ROOM_W / 2);
                loc->spawn_y = (float)(ROOM_H / 2);

                /* Heavy door at the right end (basement entrance) */
                ADD_DECOR(loc, 2370, 260, 60, 200, 55, 40, 25, "door");
                ADD_DECOR(loc, 2365, 252, 70, 12,  80, 60, 35, "doorframe");

                /* Basement door interaction trigger (trigger_id=10) */
                ADD_TRIGGER(loc, 2310, 230, 150, 220, 10, 0.0f, 0.0f);

                /* Exit left → Library (Location 2); spawn at right side of lib */
                ADD_EXIT_TRIGGER(loc, 0, 200, 60, 300,
                                 2, (float)(ROOM_W - 150), (float)(ROOM_H / 2));

                /* Colliders */
                ADD_COLLIDER(loc, 0, 0, 40, ROOM_H);
                ADD_COLLIDER(loc, ROOM_W - 40, 0, 40, ROOM_H);
                ADD_COLLIDER(loc, 0, 0, ROOM_W, 40);
                ADD_COLLIDER(loc, 0, ROOM_H - 40, ROOM_W, 40);
                break;
            }

        /* ── 2: Library ──────────────────────────────────────────────── */
            case 2: {
                loc->wall_r  = 50;  loc->wall_g  = 40;  loc->wall_b  = 30;
                loc->floor_r = 80;  loc->floor_g = 60;  loc->floor_b = 40;
                loc->ceil_r  = 25;  loc->ceil_g  = 20;  loc->ceil_b  = 15;
                loc->spawn_x = (float)(ROOM_W - 150);
                loc->spawn_y = (float)(ROOM_H / 2);

                /* Bookshelves along the walls */
                ADD_DECOR(loc,  100, 240, 200, 200,  70, 50, 30, "bookshelf");
                ADD_DECOR(loc,  350, 240, 200, 200,  65, 45, 28, "bookshelf");
                ADD_DECOR(loc, 2000, 240, 200, 200,  70, 50, 30, "bookshelf");
                ADD_DECOR(loc, 2250, 240, 200, 200,  65, 45, 28, "bookshelf");

                /* Desk with the professor's diary */
                ADD_DECOR(loc, 1200, 360, 120,  60, 90, 70, 45, "desk");
                ADD_DECOR(loc, 1220, 342,  80,  18, 160, 130, 90, "diary");

                /* Diary interaction trigger (trigger_id=1) */
                ADD_TRIGGER(loc, 1150, 300, 160, 160, 1, 0.0f, 0.0f);

                /* Exit right → Corridor (Location 1); spawn at centre of loc 1 */
                ADD_EXIT_TRIGGER(loc, ROOM_W - 60, 200, 60, 300,
                                 1, (float)(ROOM_W / 2), (float)(ROOM_H / 2));

                /* Colliders */
                ADD_COLLIDER(loc, 0, 0, 40, ROOM_H);
                ADD_COLLIDER(loc, ROOM_W - 40, 0, 40, ROOM_H);
                ADD_COLLIDER(loc, 0, 0, ROOM_W, 40);
                ADD_COLLIDER(loc, 0, ROOM_H - 40, ROOM_W, 40);
                break;
            }

        /* ── 3: Basement ─────────────────────────────────────────────── */
            case 3: {
                loc->wall_r  = 20;  loc->wall_g  = 20;  loc->wall_b  = 25;
                loc->floor_r = 40;  loc->floor_g = 35;  loc->floor_b = 30;
                loc->ceil_r  = 10;  loc->ceil_g  = 10;  loc->ceil_b  = 12;
                loc->spawn_x = 400.0f;
                loc->spawn_y = (float)FLOOR_Y;
                loc->is_danger_zone = 1;

                /* Crates and barrels */
                ADD_DECOR(loc,  600, 340,  80,  80, 60, 50, 35, "crate");
                ADD_DECOR(loc,  700, 360,  50,  60, 55, 45, 30, "barrel");
                ADD_DECOR(loc, 1800, 340,  80,  80, 60, 50, 35, "crate");
                ADD_DECOR(loc, 1900, 360,  50,  60, 55, 45, 30, "barrel");

                /* Exit left → Corridor (Location 1); spawn at centre of loc 1 */
                ADD_EXIT_TRIGGER(loc, 0, 200, 60, 300,
                                 1, (float)(ROOM_W / 2), (float)(ROOM_H / 2));

                /* Exit right → Child's Room (Location 4) */
                ADD_EXIT_TRIGGER(loc, ROOM_W - 60, 200, 60, 300,
                                 4, 150.0f, (float)(ROOM_H / 2));

                /* Colliders */
                ADD_COLLIDER(loc, 0, 0, 40, ROOM_H);
                ADD_COLLIDER(loc, ROOM_W - 40, 0, 40, ROOM_H);
                ADD_COLLIDER(loc, 0, 0, ROOM_W, 40);
                ADD_COLLIDER(loc, 0, ROOM_H - 40, ROOM_W, 40);
                break;
            }

        /* ── 4: Child's Room ─────────────────────────────────────────── */
            case 4: {
                loc->wall_r  = 60;  loc->wall_g  = 55;  loc->wall_b  = 60;
                loc->floor_r = 70;  loc->floor_g = 60;  loc->floor_b = 55;
                loc->ceil_r  = 30;  loc->ceil_g  = 25;  loc->ceil_b  = 30;
                loc->spawn_x = 150.0f;
                loc->spawn_y = (float)(ROOM_H / 2);

                /* Broken crib */
                ADD_DECOR(loc,  800, 340, 120,  80,  80, 70, 70, "crib");
                ADD_DECOR(loc,  820, 330,  80,  10, 100, 90, 90, "crib_rail");

                /* Basement key lying on the floor */
                ADD_DECOR(loc,  400, 390,  30,  12, 200, 180, 60, "key");

                /* Key interaction trigger (trigger_id=10) */
                ADD_TRIGGER(loc, 360, 340, 110, 120, 10, 0.0f, 0.0f);

                /* Exit left → Basement (Location 3) */
                ADD_EXIT_TRIGGER(loc, 0, 200, 60, 300,
                                 3, (float)(ROOM_W - 150), (float)(ROOM_H / 2));

                /* Exit right → Ritual Room (Location 5) */
                ADD_EXIT_TRIGGER(loc, ROOM_W - 60, 200, 60, 300,
                                 5, 150.0f, (float)(ROOM_H / 2));

                /* Colliders */
                ADD_COLLIDER(loc, 0, 0, 40, ROOM_H);
                ADD_COLLIDER(loc, ROOM_W - 40, 0, 40, ROOM_H);
                ADD_COLLIDER(loc, 0, 0, ROOM_W, 40);
                ADD_COLLIDER(loc, 0, ROOM_H - 40, ROOM_W, 40);
                break;
            }

        /* ── 5: Ritual Room ──────────────────────────────────────────── */
            case 5: {
                loc->wall_r  = 25;  loc->wall_g  = 15;  loc->wall_b  = 15;
                loc->floor_r = 40;  loc->floor_g = 25;  loc->floor_b = 25;
                loc->ceil_r  = 10;  loc->ceil_g  =  8;  loc->ceil_b  =  8;
                loc->spawn_x = 150.0f;
                loc->spawn_y = (float)(ROOM_H / 2);
                loc->is_danger_zone = 1;

                /* Ritual circle on the floor */
                ADD_DECOR(loc, ROOM_W/2 -  80, 360, 160,  20, 120, 20, 20, "circle");
                ADD_DECOR(loc, ROOM_W/2 -  40, 320,  80,  20, 140, 30, 30, "symbol");
                ADD_DECOR(loc, ROOM_W/2 -  40, 380,  80,  20, 140, 30, 30, "symbol");

                /* Candles at the ritual points */
                ADD_DECOR(loc, ROOM_W/2 - 100, 350,  10,  40, 220, 180, 60, "candle");
                ADD_DECOR(loc, ROOM_W/2 +  90, 350,  10,  40, 220, 180, 60, "candle");

                /* Ritual circle interaction trigger (trigger_id=20) */
                ADD_TRIGGER(loc, ROOM_W/2 - 150, 280, 300, 200, 20, 0.0f, 0.0f);

                /* Exit left → Child's Room (Location 4) */
                ADD_EXIT_TRIGGER(loc, 0, 200, 60, 300,
                                 4, (float)(ROOM_W - 150), (float)(ROOM_H / 2));

                /* Colliders */
                ADD_COLLIDER(loc, 0, 0, 40, ROOM_H);
                ADD_COLLIDER(loc, ROOM_W - 40, 0, 40, ROOM_H);
                ADD_COLLIDER(loc, 0, 0, ROOM_W, 40);
                ADD_COLLIDER(loc, 0, ROOM_H - 40, ROOM_W, 40);
                break;
            }

        default:
            break;
        }
    }
}

/* ── Room rendering ────────────────────────────────────────────────────── */
/*
 * Draws the room in three layers (ceiling, walls, floor) and then all
 * decorative objects, each offset by the camera position.
 */
void world_render_room(const Location *loc, SDL_Renderer *renderer,
                       const Camera *cam)
{
    if (!loc || !renderer || !cam) return;

    int cx = (int)cam->x;
    int cy = (int)cam->y;

    /* ── Ceiling ── */
    render_filled_rect(renderer,
        -cx, 0,
        ROOM_W, FLOOR_Y - 40,
        loc->wall_r, loc->wall_g, loc->wall_b, 255);

    /* ── Ceiling strip ── */
    render_filled_rect(renderer,
        -cx, 0,
        ROOM_W, 40,
        loc->ceil_r, loc->ceil_g, loc->ceil_b, 255);

    /* ── Wall bottom strip ── */
    render_filled_rect(renderer,
        -cx, FLOOR_Y - 40,
        ROOM_W, 40,
        (Uint8)(loc->wall_r/2), (Uint8)(loc->wall_g/2), (Uint8)(loc->wall_b/2),
        255);

    /* ── Floor ── */
    render_filled_rect(renderer,
        -cx, FLOOR_Y,
        ROOM_W, ROOM_H - FLOOR_Y,
        loc->floor_r, loc->floor_g, loc->floor_b, 255);

    /* Floor planks / detail lines */
    for (int fx = 0; fx < ROOM_W; fx += 120) {
        int lx = fx - cx;
        SDL_SetRenderDrawColor(renderer,
            (Uint8)(loc->floor_r > 15 ? loc->floor_r - 15 : 0),
            (Uint8)(loc->floor_g > 15 ? loc->floor_g - 15 : 0),
            (Uint8)(loc->floor_b > 15 ? loc->floor_b - 15 : 0),
            255);
        SDL_RenderLine(renderer, (float)lx, (float)FLOOR_Y,
                       (float)lx, (float)ROOM_H);
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
