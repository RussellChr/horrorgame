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
        loc->spawn_y = (float)(FLOOR_Y);

        switch (loc->id) {

        /* ── 0: Entrance Hall ──────────────────────────────────────── */
        case 0:
            loc->wall_r  = 40;  loc->wall_g  = 30;  loc->wall_b  = 25;
            loc->floor_r = 90;  loc->floor_g = 70;  loc->floor_b = 50;
            loc->ceil_r  = 20;  loc->ceil_g  = 15;  loc->ceil_b  = 10;
            loc->spawn_x = 300.0f; loc->spawn_y = (float)FLOOR_Y;

            /* Chandelier */
            ADD_DECOR(loc, 1200, 80,  80, 20,  160,140,100, "chandelier");
            ADD_DECOR(loc, 1230, 100,  20, 140,  80,70,50,  "chain");
            /* Front door (left) */
            ADD_DECOR(loc, 20,  400, 90, 180,  80,60,40,  "front door");
            /* Rug */
            ADD_DECOR(loc, 400, 560, 800, 40,  100,30,30,  "worn rug");
            /* Portraits */
            ADD_DECOR(loc, 600,  200, 80, 110, 50,40,30,  "portrait");
            ADD_DECOR(loc, 900,  200, 80, 110, 50,40,30,  "portrait");
            /* Stranger - interactive NPC, bright yellow so player can spot it */
            ADD_DECOR(loc, 1100, FLOOR_Y-110, 30,  60, 230,200,30, "");
            ADD_DECOR(loc, 1093, FLOOR_Y-150, 44,  44, 240,210,40, "Stranger");
            /* Staircase (right wall) */
            for (int s = 0; s < 10; s++) {
                ADD_DECOR(loc, 2200 + s*30, FLOOR_Y - s*30, 120, 20,
                          110,80,50, "");
            }
            /* Colliders: left & right walls, staircase */
            ADD_COLLIDER(loc, 0,    0,  40,  ROOM_H);
            ADD_COLLIDER(loc, ROOM_W-40, 0,  40, ROOM_H);
            ADD_COLLIDER(loc, 2200, 0, 360, FLOOR_Y); /* staircase wall */
            /* Corridor exit (right) → location 1 */
            ADD_EXIT_TRIGGER(loc, ROOM_W-60, FLOOR_Y-100, 60, 120, 1,
                             200.0f, (float)FLOOR_Y);
            /* Library exit (upper right via staircase) → location 2 */
            ADD_EXIT_TRIGGER(loc, 2190, FLOOR_Y-310, 60, 80, 2,
                             300.0f, (float)FLOOR_Y);
            /* Portrait interaction (left-side wall portrait, trigger id=30) */
            ADD_TRIGGER(loc, 580, FLOOR_Y-120, 120, 130, 30, 0.0f, 0.0f);
            /* Stranger NPC interaction (trigger id=40) */
            ADD_TRIGGER(loc, 1070, FLOOR_Y-150, 90, 160, 40, 0.0f, 0.0f);
            break;

        /* ── 1: Dark Corridor ─────────────────────────────────────── */
        case 1:
            loc->wall_r  = 18;  loc->wall_g  = 14;  loc->wall_b  = 20;
            loc->floor_r = 35;  loc->floor_g = 28;  loc->floor_b = 35;
            loc->ceil_r  = 8;   loc->ceil_g  = 6;   loc->ceil_b  = 12;
            loc->spawn_x = 200.0f; loc->spawn_y = (float)FLOOR_Y;

            /* Torches */
            ADD_DECOR(loc, 500,  320,  20, 60, 180,120,40, "torch");
            ADD_DECOR(loc, 1500, 320,  20, 60, 180,120,40, "torch");
            ADD_DECOR(loc, 2000, 320,  20, 60, 180,120,40, "torch");
            /* Halo glow under torches */
            ADD_DECOR(loc, 490,  380,  40,  8,  90,60,20,  "");
            ADD_DECOR(loc, 1490, 380,  40,  8,  90,60,20,  "");
            ADD_DECOR(loc, 1990, 380,  40,  8,  90,60,20,  "");
            /* Basement door (locked) */
            ADD_DECOR(loc, 2380, 380, 100, 200, 60,40,30, "basement door");
            ADD_DECOR(loc, 2420, 420,  20,  20, 180,140,30,"lock");
            /* Cobwebs */
            ADD_DECOR(loc, 700, 100, 60, 60, 30,30,40, "cobweb");
            ADD_DECOR(loc, 1800,100, 60, 60, 30,30,40, "cobweb");
            /* Colliders */
            ADD_COLLIDER(loc, 0, 0, 40, ROOM_H);
            ADD_COLLIDER(loc, ROOM_W-40, 0, 40, ROOM_H);
            ADD_COLLIDER(loc, 2380, 380, 100, 200); /* basement door */
            /* Exit left → Entrance Hall (0) */
            ADD_EXIT_TRIGGER(loc, 0, FLOOR_Y-100, 60, 120, 0,
                             ROOM_W-200.0f, (float)FLOOR_Y);
            /* Interact: basement door trigger id=10 */
            ADD_TRIGGER(loc, 2300, FLOOR_Y-120, 150, 130, 10, 0.0f, 0.0f);
            /* Exit right → Child's Room (4) */
            ADD_EXIT_TRIGGER(loc, ROOM_W-60, FLOOR_Y-100, 60, 120, 4,
                             200.0f, (float)FLOOR_Y);
            break;

        /* ── 2: The Library ───────────────────────────────────────── */
        case 2:
            loc->wall_r  = 55;  loc->wall_g  = 30;  loc->wall_b  = 20;
            loc->floor_r = 70;  loc->floor_g = 50;  loc->floor_b = 35;
            loc->ceil_r  = 25;  loc->ceil_g  = 15;  loc->ceil_b  = 10;
            loc->spawn_x = 500.0f; loc->spawn_y = (float)FLOOR_Y;

            /* Bookshelves along back wall */
            for (int b = 0; b < 8; b++) {
                ADD_DECOR(loc, 100 + b*290, 80, 270, 450, 60,35,20, "bookshelf");
                /* Book spines (detail rows) */
                for (int r = 0; r < 6; r++) {
                    Uint8 br = (Uint8)(80 + (b*17+r*31) % 80);
                    Uint8 bg = (Uint8)(40 + (b*13+r*23) % 40);
                    ADD_DECOR(loc, 105+b*290, 90+r*60, 260, 50, br,bg,15,"");
                }
            }
            /* Reading desk */
            ADD_DECOR(loc, 1100, FLOOR_Y-120, 300, 40, 90,65,40, "desk");
            ADD_DECOR(loc, 1120, FLOOR_Y-90,  260, 80, 80,55,30, "desk front");
            /* Diary item highlight */
            ADD_DECOR(loc, 1180, FLOOR_Y-135, 80, 15, 220,200,150,"diary");
            /* Chair */
            ADD_DECOR(loc, 1070, FLOOR_Y-100, 40, 90, 70,50,30, "chair");
            /* Colliders */
            ADD_COLLIDER(loc, 0,    0, 40, ROOM_H);
            ADD_COLLIDER(loc, ROOM_W-40, 0, 40, ROOM_H);
            for (int b = 0; b < 8; b++)
                ADD_COLLIDER(loc, 100+b*290, 80, 270, 450);
            ADD_COLLIDER(loc, 1100, FLOOR_Y-120, 300, 120);
            /* Exit down → Entrance Hall (0) */
            ADD_EXIT_TRIGGER(loc, 0, FLOOR_Y-100, 60, 120, 0,
                             ROOM_W-200.0f, (float)FLOOR_Y);
            /* Diary interact trigger id=1 */
            ADD_TRIGGER(loc, 1160, FLOOR_Y-155, 120, 60, 1, 0.0f, 0.0f);
            break;

        /* ── 3: Basement ──────────────────────────────────────────── */
        case 3:
            loc->wall_r  = 22;  loc->wall_g  = 20;  loc->wall_b  = 25;
            loc->floor_r = 40;  loc->floor_g = 38;  loc->floor_b = 45;
            loc->ceil_r  = 10;  loc->ceil_g  = 8;   loc->ceil_b  = 15;
            loc->spawn_x = 400.0f; loc->spawn_y = (float)FLOOR_Y;

            /* Stone pillars */
            for (int p = 0; p < 4; p++)
                ADD_DECOR(loc, 300+p*500, 200, 60, FLOOR_Y-200, 50,48,55,"pillar");
            /* Crates */
            ADD_DECOR(loc, 600, FLOOR_Y-90, 90, 90, 80,65,45, "crate");
            ADD_DECOR(loc, 700, FLOOR_Y-70, 70, 70, 75,60,40, "crate");
            /* Chains on wall */
            ADD_DECOR(loc, 1400, 300, 10, 200, 80,80,90, "chain");
            ADD_DECOR(loc, 1420, 300, 10, 200, 80,80,90, "chain");
            /* Water pool */
            ADD_DECOR(loc, 1700, FLOOR_Y-30, 200, 30, 30,40,60, "pool");
            /* Passage to ritual room (glowing) */
            ADD_DECOR(loc, ROOM_W-100, FLOOR_Y-180, 100, 180, 40,20,60,"passage");
            ADD_DECOR(loc, ROOM_W-90,  FLOOR_Y-175,  80, 170, 50,25,70,"");
            /* Colliders */
            ADD_COLLIDER(loc, 0, 0, 40, ROOM_H);
            ADD_COLLIDER(loc, ROOM_W-40, 0, 40, ROOM_H);
            for (int p = 0; p < 4; p++)
                ADD_COLLIDER(loc, 300+p*500, 200, 60, FLOOR_Y-200);
            ADD_COLLIDER(loc, 600, FLOOR_Y-90, 90, 90);
            ADD_COLLIDER(loc, 700, FLOOR_Y-70, 70, 70);
            /* Exit up → Corridor (1) */
            ADD_EXIT_TRIGGER(loc, 0, FLOOR_Y-100, 60, 120, 1,
                             ROOM_W-200.0f, (float)FLOOR_Y);
            /* Exit right → Ritual Room (5) */
            ADD_EXIT_TRIGGER(loc, ROOM_W-60, FLOOR_Y-180, 60, 180, 5,
                             200.0f, (float)FLOOR_Y);
            break;

        /* ── 4: Child's Room ──────────────────────────────────────── */
        case 4:
            loc->wall_r  = 120; loc->wall_g  = 100; loc->wall_b  = 110;
            loc->floor_r = 100; loc->floor_g = 85;  loc->floor_b = 75;
            loc->ceil_r  = 60;  loc->ceil_g  = 50;  loc->ceil_b  = 55;
            loc->spawn_x = 500.0f; loc->spawn_y = (float)FLOOR_Y;

            /* Wallpaper pattern (stripes) */
            for (int s = 0; s < 16; s++)
                ADD_DECOR(loc, s*160, 0, 10, FLOOR_Y, 100,85,100,"");
            /* Bed */
            ADD_DECOR(loc, 200, FLOOR_Y-100, 300, 80, 180,160,160, "bed");
            ADD_DECOR(loc, 210, FLOOR_Y-140, 280, 40, 200,180,180, "pillow");
            ADD_DECOR(loc, 180, FLOOR_Y-100, 30, 100, 160,130,100, "bedpost");
            ADD_DECOR(loc, 490, FLOOR_Y-100, 30, 100, 160,130,100, "bedpost");
            /* Toy box */
            ADD_DECOR(loc, 700, FLOOR_Y-80, 120, 80, 180,120,80, "toybox");
            /* Teddy bear */
            ADD_DECOR(loc, 740, FLOOR_Y-130, 50, 50, 160,110,80, "teddy");
            /* Drawing on wall */
            ADD_DECOR(loc, 1000, 200, 150, 120, 220,200,180, "drawing");
            /* Key item highlight */
            ADD_DECOR(loc, 730, FLOOR_Y-95,  30, 12, 220,190,50, "key");
            /* Window */
            ADD_DECOR(loc, 1600, 120, 160, 180, 60,80,110, "window");
            ADD_DECOR(loc, 1620, 130, 120, 160, 80,100,140, "glass");
            /* Colliders */
            ADD_COLLIDER(loc, 0, 0, 40, ROOM_H);
            ADD_COLLIDER(loc, ROOM_W-40, 0, 40, ROOM_H);
            ADD_COLLIDER(loc, 200, FLOOR_Y-100, 300, 100);
            ADD_COLLIDER(loc, 700, FLOOR_Y-80,  120, 80);
            /* Exit left → Corridor (1) */
            ADD_EXIT_TRIGGER(loc, 0, FLOOR_Y-100, 60, 120, 1,
                             ROOM_W-300.0f, (float)FLOOR_Y);
            /* Key interact trigger id=10 */
            ADD_TRIGGER(loc, 710, FLOOR_Y-110, 90, 40, 10, 0.0f, 0.0f);
            break;

        /* ── 5: Ritual Room ───────────────────────────────────────── */
        case 5:
            loc->wall_r  = 25;  loc->wall_g  = 10;  loc->wall_b  = 30;
            loc->floor_r = 35;  loc->floor_g = 15;  loc->floor_b = 40;
            loc->ceil_r  = 10;  loc->ceil_g  = 5;   loc->ceil_b  = 20;
            loc->spawn_x = 400.0f; loc->spawn_y = (float)FLOOR_Y;

            /* Ritual circle on floor */
            ADD_DECOR(loc, 1100, FLOOR_Y-10, 360, 10, 180,0,0,   "circle");
            ADD_DECOR(loc, 1060, FLOOR_Y-50, 440, 10, 180,0,0,   "circle");
            ADD_DECOR(loc, 1040, FLOOR_Y-90, 480, 10, 180,0,0,   "");
            /* Candles */
            for (int c = 0; c < 8; c++) {
                int cx = 1100 + c*50;
                ADD_DECOR(loc, cx, FLOOR_Y-80, 8, 50, 220,200,150, "candle");
                ADD_DECOR(loc, cx, FLOOR_Y-90, 4,  8, 255,240,100, "flame");
            }
            /* Altar */
            ADD_DECOR(loc, 1200, FLOOR_Y-160, 200,  30, 80,10,10, "altar");
            ADD_DECOR(loc, 1220, FLOOR_Y-260, 160, 100, 60, 5, 5, "altar top");
            /* Symbols on wall */
            ADD_DECOR(loc, 800, 200, 80, 100, 90,20,90, "symbol");
            ADD_DECOR(loc, 1600,200, 80, 100, 90,20,90, "symbol");
            ADD_DECOR(loc, 1200,150,120,  80, 100,10,100,"symbol");
            /* Cracks in wall */
            ADD_DECOR(loc, 300, 100, 4, 300, 50,30,60,  "crack");
            ADD_DECOR(loc, 2100,150, 4, 250, 50,30,60,  "crack");
            /* Colliders */
            ADD_COLLIDER(loc, 0, 0, 40, ROOM_H);
            ADD_COLLIDER(loc, ROOM_W-40, 0, 40, ROOM_H);
            ADD_COLLIDER(loc, 1200, FLOOR_Y-260, 200, 260);
            /* Exit left → Basement (3) */
            ADD_EXIT_TRIGGER(loc, 0, FLOOR_Y-100, 60, 120, 3,
                             ROOM_W-300.0f, (float)FLOOR_Y);
            /* Ritual circle interact trigger id=20 */
            ADD_TRIGGER(loc, 1040, FLOOR_Y-100, 480, 110, 20, 0.0f, 0.0f);
            break;

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
