#include "game.h"
#include "player.h"
#include "world.h"
#include "story.h"
#include "dialogue.h"
#include "monologue.h"
#include "ui.h"
#include "render.h"
#include "camera.h"
#include "collision.h"
#include "npc.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ── Monitor passcode constants ────────────────────────────────────────── */
#define MONITOR_PANEL_X      685
#define MONITOR_PANEL_Y      415
#define MONITOR_PANEL_W       75
#define MONITOR_PANEL_H      119
#define PASSCODE_CORRECT     "3643"
/* Display buffer: 4 digits alternating with spaces + null  ("_ _ _ _\0") */
#define PASSCODE_DISPLAY_SIZE  8

/* ── Helpers ───────────────────────────────────────────────────────────── */

static Button make_button(float x, float y, float w, float h,
                          const char *text)
{
    Button b;
    b.rect.x = x; b.rect.y = y;
    b.rect.w = w; b.rect.h = h;
    b.text   = text;
    b.is_hovered = 0;
    return b;
}

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

Game *game_init(SDL_Window *window, SDL_Renderer *renderer)
{
    Game *g = calloc(1, sizeof(Game));
    if (!g) return NULL;

    g->window   = window;
    g->renderer = renderer;
    g->state    = GAME_STATE_MENU;
    g->running  = 1;

    float bw = 240.0f, bh = 54.0f;
    float bx = (WINDOW_W - bw) / 2.0f;
    g->buttons[0] = make_button(60.0f, 500.0f, bw, bh, "New Game");
    g->buttons[1] = make_button(60.0f, 500.0f + (bh + 10.0f), bw, bh, "Settings");
    g->buttons[2] = make_button(60.0f, 500.0f + 2.0f * (bh + 10.0f), bw, bh, "Quit");
    g->pause_buttons[0] = make_button(bx, 310.0f, bw, bh, "Resume");
    g->pause_buttons[1] = make_button(bx, 380.0f, bw, bh, "Quit to Menu");

    /* Settings defaults */
    g->volume     = 100.0f;
    g->brightness = 100.0f;
    g->settings_focus = 0;
    float sw = 400.0f, sh = 24.0f;
    float sx = (WINDOW_W - sw) / 2.0f + 60.0f;   /* offset right to leave room for label */
    slider_init(&g->settings_volume_slider,     sx, 300.0f, sw, sh, 0.0f, 100.0f, 100.0f);
    slider_init(&g->settings_brightness_slider, sx, 380.0f, sw, sh, 0.0f, 100.0f, 100.0f);
    float bbw = 200.0f, bbh = 48.0f;
    g->settings_back_button = make_button(
        (WINDOW_W - bbw) / 2.0f, 460.0f, bbw, bbh, "Back");

    g->last_ticks = SDL_GetTicks();
    g->keys       = SDL_GetKeyboardState(NULL);

    /* Load the dialogue box background image */
    dialogue_load_texture(&g->dialogue_state, renderer, "assets/dialogue.png");

    /* Load inner monologue file */
    monologue_load(&g->monologue_file, "assets/dialogue/monologues.txt");

    /* Load Title Screen*/
    g->title_screen_texture = render_load_texture(renderer, "assets/title_screen.png");

    /* Load locker view */
    g->locker_texture      = render_load_texture(renderer, "assets/locker.png");
    g->note_locker_texture = render_load_texture(renderer, "assets/note_locker.png");
    g->monitor_zoom_texture = render_load_texture(renderer, "assets/monitor_zoom.png");

    /* Load inventory item icons */
    g->item_flashlight_texture = render_load_texture(renderer, "assets/flashlight.png");
    g->item_gasmask_texture    = render_load_texture(renderer, "assets/gasmask.png");

    /* Create full-screen light-mask render target for archive room darkness */
    g->dark_overlay = SDL_CreateTexture(renderer,
                                        SDL_PIXELFORMAT_RGBA8888,
                                        SDL_TEXTUREACCESS_TARGET,
                                        WINDOW_W, WINDOW_H);
    if (!g->dark_overlay)
        SDL_Log("game_init: failed to create dark_overlay texture: %s",
                SDL_GetError());

    return g;
}

void game_cleanup(Game *game)
{
    if (!game) return;
    if (game->player)        player_destroy(game->player);
    if (game->world)         world_destroy(game->world);
    if (game->story)         story_destroy(game->story);
    if (game->dialogue_tree) dialogue_tree_destroy(game->dialogue_tree);
    dialogue_unload_texture(&game->dialogue_state);
    render_texture_destroy(game->title_screen_texture);
    render_texture_destroy(game->locker_texture);
    render_texture_destroy(game->note_locker_texture);
    render_texture_destroy(game->monitor_zoom_texture);
    render_texture_destroy(game->item_flashlight_texture);
    render_texture_destroy(game->item_gasmask_texture);
    render_texture_destroy(game->dark_overlay);
    if (game->npc_manager) npc_manager_destroy(game->npc_manager);
    free(game);
}

/* ── State transitions ─────────────────────────────────────────────────── */

void game_start_new(Game *game)
{
    if (!game) return;

    game->player = player_create("Alex");
    game->world  = world_create();
    game->story  = story_create();

    /* Load idle animations from assets/player/ */
    game->player->idle_texture_north = render_load_texture(game->renderer, "assets/player/player_idle_north.png");
    game->player->idle_texture_south = render_load_texture(game->renderer, "assets/player/player_idle_south.png");
    game->player->idle_texture_east  = render_load_texture(game->renderer, "assets/player/player_idle_east.png");
    game->player->idle_texture_west  = render_load_texture(game->renderer, "assets/player/player_idle_west.png");

    /* Load walking animations from assets/player/ */
    game->player->walk_frames_north[0] = render_load_texture(game->renderer, "assets/player/player_walk_north_0.png");
    game->player->walk_frames_north[1] = render_load_texture(game->renderer, "assets/player/player_walk_north_1.png");
    game->player->walk_frames_south[0] = render_load_texture(game->renderer, "assets/player/player_walk_south_0.png");
    game->player->walk_frames_south[1] = render_load_texture(game->renderer, "assets/player/player_walk_south_1.png");
    game->player->walk_frames_east[0]  = render_load_texture(game->renderer, "assets/player/player_walk_east_0.png");
    game->player->walk_frames_east[1]  = render_load_texture(game->renderer, "assets/player/player_walk_east_1.png");
    game->player->walk_frames_west[0]  = render_load_texture(game->renderer, "assets/player/player_walk_west_0.png");
    game->player->walk_frames_west[1]  = render_load_texture(game->renderer, "assets/player/player_walk_west_1.png");

    /* Initialize animation state */
    game->player->current_direction = DIRECTION_EAST;  /* Start facing east */
    game->player->frame_index       = 0;
    game->player->frame_timer       = 0.0f;
    game->player->frame_duration    = 0.15f;

    story_populate_world(game->world, "assets/locations.txt");
    world_setup_rooms(game->world, game->renderer);

    /* ── Hallway patrol enemy ── */
    game->npc_manager = npc_manager_create();
    if (game->npc_manager) {
        static const PatrolWaypoint hallway_wps[] = {
            {1094.0f, 316.0f},
            {1091.0f, 580.0f},
            { 650.0f, 388.0f},
            { 650.0f, 544.0f},
            { 261.0f, 542.0f},
            { 261.0f, 236.0f},
        };
        NPC *enemy = npc_create(100, "Enemy", hallway_wps[0].x, hallway_wps[0].y,
                                NPC_TYPE_HOSTILE);
        if (enemy) {
            enemy->location_id = 2;   /* Hallway */
            enemy->w = 16;
            enemy->h = 16;
            npc_set_patrol_waypoints(enemy, hallway_wps,
                                     (int)(sizeof(hallway_wps) / sizeof(hallway_wps[0])));
            npc_manager_add(game->npc_manager, enemy);
            npc_destroy(enemy);
        }
    }

    game->player->current_location_id = 3;  /* Start in Room 3 */

    Location *start = world_get_location(game->world,
                                         game->player->current_location_id);
    int start_w = start ? start->room_width  : ROOM_W;
    int start_h = start ? start->room_height : ROOM_H;
    camera_init(&game->camera, WINDOW_W, WINDOW_H, start_w, start_h);

    if (start) {
        game->player->x = 725.0f;  /* Your X coordinate */
        game->player->y = 390.0f;  /* Your Y coordinate */
    }
    camera_snap(&game->camera, game->player->x, game->player->y);

    game->state                   = GAME_STATE_PLAYING;
    game->near_interactive        = 0;
    game->interactive_trigger_id  = -1;
    game->selected_inventory_slot = 0;

    /* Show the opening inner monologue if one is defined */
    const MonologueSection *open_mono =
        monologue_find(&game->monologue_file, "game_start");
    if (open_mono) {
        game->dialogue_tree = monologue_to_dialogue_tree(open_mono);
        if (game->dialogue_tree)
            game_start_dialogue(game, 0);
    }
}

void game_change_location(Game *game, int location_id,
                          float spawn_x, float spawn_y)
{
    if (!game || !game->world) return;

    Location *prev = world_get_location(game->world,
                                        game->player->current_location_id);
    if (prev) prev->visited = 1;

    game->player->current_location_id = location_id;
    game->player->x = spawn_x;
    game->player->y = spawn_y;
    game->player->vx = 0.0f;
    game->player->vy = 0.0f;

    /* Track first-visit story flags for the objective system */
    if (location_id == LOCATION_LAB)
        player_set_flag(game->player, FLAG_ENTERED_LAB);
    else if (location_id == LOCATION_HALLWAY)
        player_set_flag(game->player, FLAG_ENTERED_HALLWAY);

    Location *next = world_get_location(game->world, location_id);
    if (next) {
        game->camera.world_w = next->room_width;
        game->camera.world_h = next->room_height;
    }

    camera_snap(&game->camera, spawn_x, spawn_y);
}

void game_start_dialogue(Game *game, int node_id)
{
    if (!game || !game->dialogue_tree) return;
    dialogue_state_init(&game->dialogue_state, game->dialogue_tree, node_id);
    game->state = GAME_STATE_DIALOGUE;
}

void game_end_dialogue(Game *game)
{
    if (!game) return;

    /* Check (and consume) the transient note flag before destroying the tree */
    int show_note = game->player &&
                    player_check_flag(game->player, FLAG_SECURITY_NOTE_READ);
    if (show_note && game->player)
        player_clear_flag(game->player, FLAG_SECURITY_NOTE_READ);

    if (game->dialogue_tree) {
        dialogue_tree_destroy(game->dialogue_tree);
        game->dialogue_tree = NULL;
    }

    if (show_note) {
        game->show_note_locker = 1;
        game->state = GAME_STATE_LOCKER;
    } else {
        game->state = GAME_STATE_PLAYING;
    }
}

/* ── Interaction handler ───────────────────────────────────────────────── */

/* Apply story flag from the currently selected dialogue choice. */
static void apply_dialogue_choice_flag(Game *game)
{
    if (!game || !game->player || !game->dialogue_state.text_complete) return;

    const DialogueChoice *ch = dialogue_state_get_selected(
        &game->dialogue_state, 0);
    if (!ch) return;

    if (ch->story_flag)
        game->player->flags |= (uint32_t)ch->story_flag;
}

/* Set game->dialogue_tree: try the named monologue section first; fall back
 * to the location-based procedural tree when no monologue is found. */
static void set_dialogue_tree(Game *game,
                               const char *monologue_id,
                               int fallback_location_id)
{
    if (monologue_id) {
        const MonologueSection *ms =
            monologue_find(&game->monologue_file, monologue_id);
        if (ms) {
            game->dialogue_tree = monologue_to_dialogue_tree(ms);
            if (game->dialogue_tree) return;
        }
    }
    game->dialogue_tree = dialogue_build_for_location(fallback_location_id);
}

static void handle_interaction(Game *game)
{
    if (!game || !game->player || !game->world) return;

    int loc_id = game->player->current_location_id;
    int tid    = game->interactive_trigger_id;

    /* Guard: don't open duplicate dialogue */
    if (game->dialogue_tree) return;

    /* Locker interaction (Hallway, trigger 60) */
    if (tid == 60 && loc_id == 2) {
        game->state = GAME_STATE_LOCKER;
        return;
    }

    /* ── Hallway interactions (loc 2) ──────────────────────────────────── */
    if (loc_id == 2) {
        if (tid == 80) {
            /* Tile 8: interactable – nothing here */
            set_dialogue_tree(game, "hallway_nothing", 2);
        } else if (tid == 81) {
            /* Tile 9: gas mask pickup */
            if (!player_check_flag(game->player, FLAG_HALLWAY_GASMASK_COLLECTED)) {
                player_set_flag(game->player, FLAG_HALLWAY_GASMASK_COLLECTED);
                Item gm;
                strncpy(gm.name, "Gas Mask", ITEM_NAME_MAX - 1);
                gm.name[ITEM_NAME_MAX - 1] = '\0';
                strncpy(gm.description, "GasMask found", ITEM_DESC_MAX - 1);
                gm.description[ITEM_DESC_MAX - 1] = '\0';
                gm.id     = ITEM_ID_GASMASK;
                gm.usable = 0;
                player_add_item(game->player, &gm);
                set_dialogue_tree(game, "hallway_gasmask", 2);
            } else {
                set_dialogue_tree(game, "hallway_nothing", 2);
            }
        } else if (tid == 82) {
            /* Tile 10: flashlight pickup */
            if (!player_check_flag(game->player, FLAG_HALLWAY_FLASHLIGHT_COLLECTED)) {
                player_set_flag(game->player, FLAG_HALLWAY_FLASHLIGHT_COLLECTED);
                Item fl;
                strncpy(fl.name, "Flashlight", ITEM_NAME_MAX - 1);
                fl.name[ITEM_NAME_MAX - 1] = '\0';
                strncpy(fl.description, "flashlight found", ITEM_DESC_MAX - 1);
                fl.description[ITEM_DESC_MAX - 1] = '\0';
                fl.id     = ITEM_ID_FLASHLIGHT;
                fl.usable = 1;
                player_add_item(game->player, &fl);
                set_dialogue_tree(game, "hallway_flashlight", 2);
            } else {
                set_dialogue_tree(game, "hallway_nothing", 2);
            }
        }
        if (game->dialogue_tree)
            game_start_dialogue(game, 0);
        return;
    }

    /* ── Hibernation room interactions (loc 3) ─────────────────────────── */
    if (loc_id == 3) {
        if (tid == 71) {
            /* Tile 1: zonk – nothing here */
            set_dialogue_tree(game, "hibern_zonk", 3);
        } else if (tid == 72) {
            /* Tile 2: power cell pickup */
            if (!player_check_flag(game->player, FLAG_HIBERN_POWERCELL_COLLECTED)) {
                player_set_flag(game->player, FLAG_HIBERN_POWERCELL_COLLECTED);
                Item pc;
                strncpy(pc.name, "Power Cell", ITEM_NAME_MAX - 1);
                pc.name[ITEM_NAME_MAX - 1] = '\0';
                strncpy(pc.description, "A cylindrical power cell.", ITEM_DESC_MAX - 1);
                pc.description[ITEM_DESC_MAX - 1] = '\0';
                pc.id     = ITEM_ID_POWERCELL;
                pc.usable = 0;
                player_add_item(game->player, &pc);
                set_dialogue_tree(game, "hibern_powercell", 3);
            } else {
                set_dialogue_tree(game, "hibern_zonk", 3);
            }
        } else if (tid == 73) {
            /* Tile 3: power cell slot */
            if (player_has_item(game->player, ITEM_ID_POWERCELL)) {
                player_remove_item(game->player, ITEM_ID_POWERCELL);
                player_set_flag(game->player, FLAG_HIBERN_POWERCELL_PLACED);

                /* Open the door: remove tile-5 collision barrier and convert
                   the tile-5 interactive triggers into real exit triggers so
                   the player can walk through to the hallway.
                   Note: door colliders are always the last ones added in
                   world_setup_rooms (after map_build_colliders and before any
                   trigger registration), so truncating to door_collider_start
                   removes exactly and only the door colliders. */
                Location *hloc = world_get_location(game->world, 3);
                if (hloc) {
                    hloc->collider_count = hloc->door_collider_start;
                    for (int i = 0; i < hloc->trigger_count; i++) {
                        if (hloc->triggers[i].trigger_id == 75) {
                            hloc->triggers[i].target_location_id = 2;
                        }
                    }
                }

                set_dialogue_tree(game, "hibern_slot_place", 3);
            } else {
                set_dialogue_tree(game, "hibern_slot_empty", 3);
            }
        } else if (tid == 74) {
            /* Tile 4: flavor description – accessible any time */
            set_dialogue_tree(game, "hibern_pods_opened", 3);
        } else if (tid == 75) {
            /* Tile 5: door – still interactive = still locked */
            set_dialogue_tree(game, "hibern_door_locked", 3);
        }
        if (game->dialogue_tree)
            game_start_dialogue(game, 0);
        return;
    }

    /* Portrait interaction (Entrance Hall, trigger 30) */
    if (tid == 30 && loc_id == 0) {
        set_dialogue_tree(game, "portrait", 30);
    }
    /* Stranger NPC interaction (Entrance Hall, trigger 40) */
    else if (tid == 40 && loc_id == 0) {
        set_dialogue_tree(game, "stranger", 40);
    }
    /* ── Security room interactions (loc 5) ────────────────────────────── */
    else if (loc_id == 5) {
        if (tid == 91) {
            /* Tile 2: readable note – Yes/No prompt */
            DialogueTree *tree = dialogue_tree_create();
            if (tree) {
                DialogueNode *q = dialogue_add_node(tree, 0, "",
                    "Would you like to read it?", 0);
                if (q) {
                    DialogueChoice yes;
                    memset(&yes, 0, sizeof(yes));
                    yes.id           = 0;
                    yes.next_node_id = -1; /* sentinel: detected in event handler to end dialogue */
                    yes.story_flag   = FLAG_SECURITY_NOTE_READ;
                    strncpy(yes.text, "Yes", DIALOGUE_TEXT_MAX - 1);
                    dialogue_add_choice(q, &yes);

                    DialogueChoice no;
                    memset(&no, 0, sizeof(no));
                    no.id           = 1;
                    no.next_node_id = -1; /* sentinel: detected in event handler to end dialogue */
                    no.story_flag   = 0;
                    strncpy(no.text, "No", DIALOGUE_TEXT_MAX - 1);
                    dialogue_add_choice(q, &no);
                }
                game->dialogue_tree = tree;
                game_start_dialogue(game, 0);
            }
        } else if (tid == 92) {
            /* Tile 4: monitor screen – show zoomed image */
            game->show_monitor_zoom = 1;
            game->state = GAME_STATE_LOCKER;
        }
        return;
    }
    /* Default interaction */
    else {
        game->dialogue_tree = dialogue_build_for_location(loc_id);
    }

    if (game->dialogue_tree)
        game_start_dialogue(game, 0);
}

/* ── Per-frame event handling ──────────────────────────────────────────── */

void game_handle_event(Game *game, SDL_Event *event)
{
    if (!game || !event) return;

    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        game->mouse_x = event->motion.x;
        game->mouse_y = event->motion.y;
    }
    game->mouse_clicked = 0;
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        game->mouse_clicked = 1;

    switch (game->state) {

    case GAME_STATE_MENU:
        for (int i = 0; i < 3; i++)
            button_update_hover(&game->buttons[i],
                                game->mouse_x, game->mouse_y);

        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            for (int i = 0; i < 3; i++) {
                if (button_is_clicked(&game->buttons[i],
                                      game->mouse_x, game->mouse_y)) {
                    if      (i == 0) game_start_new(game);
                    else if (i == 1) game->state = GAME_STATE_SETTINGS;
                    else if (i == 2) game->state = GAME_STATE_QUIT;
                }
            }
        }
        if (event->type == SDL_EVENT_KEY_DOWN) {
            switch (event->key.key) {
            case SDLK_UP:
                if (game->current_menu_choice > 0)
                    game->current_menu_choice--;
                break;
            case SDLK_DOWN:
                if (game->current_menu_choice < 2)
                    game->current_menu_choice++;
                break;
            case SDLK_RETURN:
                if      (game->current_menu_choice == 0) game_start_new(game);
                else if (game->current_menu_choice == 1) game->state = GAME_STATE_SETTINGS;
                else if (game->current_menu_choice == 2) game->state = GAME_STATE_QUIT;
                break;
            default: break;
            }
        }
        break;

    case GAME_STATE_PLAYING:
        if (event->type == SDL_EVENT_KEY_DOWN) {
            switch (event->key.key) {
            case SDLK_I:
                game->state = GAME_STATE_INVENTORY;
                break;
            case SDLK_ESCAPE:
                game->state = GAME_STATE_PAUSE;
                break;
            case SDLK_E:
                if (game->near_interactive)
                    handle_interaction(game);
                break;
            default: break;
            }
        }
        break;

    case GAME_STATE_LOCKER:
        if (game->show_monitor_zoom && !game->passcode_active) {
            /* Check if the invisible monitor panel rect was clicked */
            if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                float mx = game->mouse_x, my = game->mouse_y;
                if (mx >= MONITOR_PANEL_X && mx <= MONITOR_PANEL_X + MONITOR_PANEL_W &&
                    my >= MONITOR_PANEL_Y && my <= MONITOR_PANEL_Y + MONITOR_PANEL_H) {
                    game->passcode_active    = 1;
                    game->passcode_input_len = 0;
                    game->passcode_input[0]  = '\0';
                    game->passcode_wrong     = 0;
                }
            }
        }
        if (game->passcode_active) {
            if (event->type == SDL_EVENT_KEY_DOWN) {
                SDL_Keycode  k  = event->key.key;
                SDL_Scancode sc = event->key.scancode;
                /* If correct code was just shown, any key dismisses the overlay */
                if (game->passcode_correct) {
                    game->passcode_active    = 0;
                    game->passcode_correct   = 0;
                    game->passcode_input_len = 0;
                    game->passcode_input[0]  = '\0';
                    game->passcode_wrong     = 0;
                } else if (k == SDLK_ESCAPE) {
                    /* Close passcode overlay */
                    game->passcode_active    = 0;
                    game->passcode_input_len = 0;
                    game->passcode_input[0]  = '\0';
                    game->passcode_wrong     = 0;
                } else if (k == SDLK_BACKSPACE && game->passcode_input_len > 0) {
                    game->passcode_input_len--;
                    game->passcode_input[game->passcode_input_len] = '\0';
                    game->passcode_wrong = 0;
                } else if (game->passcode_input_len < 4) {
                    /* Use scancode for digit detection – reliable across SDL3
                       keycode convention changes.
                       SDL_SCANCODE_1-9 = 30-38, SDL_SCANCODE_0 = 39
                       SDL_SCANCODE_KP_1-9 = 89-97, SDL_SCANCODE_KP_0 = 98 */
                    char digit = 0;
                    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
                        digit = (char)('1' + (sc - SDL_SCANCODE_1));
                    else if (sc == SDL_SCANCODE_0)
                        digit = '0';
                    else if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_9)
                        digit = (char)('1' + (sc - SDL_SCANCODE_KP_1));
                    else if (sc == SDL_SCANCODE_KP_0)
                        digit = '0';
                    if (digit) {
                        game->passcode_input[game->passcode_input_len++] = digit;
                        game->passcode_input[game->passcode_input_len] = '\0';
                        game->passcode_wrong = 0;
                        /* Auto-submit when 4 digits entered */
                        if (game->passcode_input_len == 4) {
                            if (strcmp(game->passcode_input, PASSCODE_CORRECT) == 0) {
                                /* Correct – show success message; overlay stays open */
                                game->passcode_correct = 1;
                            } else {
                                game->passcode_wrong = 1;
                            }
                        }
                    }
                }
            }
            break;  /* Consume all events while passcode is open */
        }
        if (event->type == SDL_EVENT_KEY_DOWN) {
            if (event->key.key == SDLK_ESCAPE ||
                event->key.key == SDLK_E) {
                game->show_note_locker  = 0;
                game->show_monitor_zoom = 0;
                game->passcode_active    = 0;
                game->passcode_input_len = 0;
                game->passcode_input[0]  = '\0';
                game->passcode_wrong     = 0;
                game->passcode_correct   = 0;
                game->state = GAME_STATE_PLAYING;
            }
        }
        break;

    case GAME_STATE_DIALOGUE:
        if (event->type == SDL_EVENT_KEY_DOWN) {
            /* Navigate choices with UP / DOWN */
            if (event->key.key == SDLK_UP &&
                game->dialogue_state.text_complete) {
                if (game->dialogue_state.selected_choice > 0)
                    game->dialogue_state.selected_choice--;
            }
            if (event->key.key == SDLK_DOWN &&
                game->dialogue_state.text_complete) {
                const DialogueNode *_n = dialogue_get_node(
                    game->dialogue_state.tree,
                    game->dialogue_state.current_node_id);
                if (_n && game->dialogue_state.selected_choice < _n->choice_count - 1)
                    game->dialogue_state.selected_choice++;
            }
            if (event->key.key == SDLK_RETURN ||
                event->key.key == SDLK_SPACE  ||
                event->key.key == SDLK_E) {
                /* Apply story flag from the chosen option before advancing */
                apply_dialogue_choice_flag(game);
                int cont = dialogue_state_advance(&game->dialogue_state, 0);
                /* Also end dialogue if the next node doesn't exist (sentinel -1) */
                if (cont && !dialogue_get_node(game->dialogue_state.tree,
                                               game->dialogue_state.current_node_id))
                    cont = 0;
                if (!cont)
                    game_end_dialogue(game);
            }
            if (event->key.key == SDLK_ESCAPE)
                game_end_dialogue(game);
        }
        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            /* Apply story flag before advancing on mouse click too */
            apply_dialogue_choice_flag(game);
            int cont = dialogue_state_advance(&game->dialogue_state, 0);
            /* Also end dialogue if the next node doesn't exist (sentinel -1) */
            if (cont && !dialogue_get_node(game->dialogue_state.tree,
                                           game->dialogue_state.current_node_id))
                cont = 0;
            if (!cont)
                game_end_dialogue(game);
        }
        break;

    case GAME_STATE_INVENTORY:
        if (event->type == SDL_EVENT_KEY_DOWN) {
            if (event->key.key == SDLK_ESCAPE ||
                event->key.key == SDLK_I)
                game->state = GAME_STATE_PLAYING;

            /* Use selected item */
            if (event->key.key == SDLK_U) {
                int sel = game->selected_inventory_slot;
                if (sel >= 0 && sel < game->player->inventory_count) {
                    const Item *it = &game->player->inventory[sel];
                    if (it->usable && it->id == ITEM_ID_FLASHLIGHT) {
                        game->flashlight_active = !game->flashlight_active;
                        game->state = GAME_STATE_PLAYING;
                    }
                }
            }

            /* Grid navigation: 4 columns */
#define INV_NAV_COLS 4
            int *slot = &game->selected_inventory_slot;
            int  cap  = INVENTORY_CAPACITY;
            if (event->key.key == SDLK_LEFT) {
                if (*slot % INV_NAV_COLS > 0) (*slot)--;
            }
            if (event->key.key == SDLK_RIGHT) {
                if (*slot % INV_NAV_COLS < INV_NAV_COLS - 1 &&
                    *slot + 1 < cap)
                    (*slot)++;
            }
            if (event->key.key == SDLK_UP) {
                if (*slot >= INV_NAV_COLS) *slot -= INV_NAV_COLS;
            }
            if (event->key.key == SDLK_DOWN) {
                if (*slot + INV_NAV_COLS < cap)
                    *slot += INV_NAV_COLS;
            }
#undef INV_NAV_COLS
        }
        break;

    case GAME_STATE_PAUSE:
        for (int i = 0; i < 2; i++)
            button_update_hover(&game->pause_buttons[i],
                                game->mouse_x, game->mouse_y);
        if (event->type == SDL_EVENT_KEY_DOWN) {
            if (event->key.key == SDLK_ESCAPE)
                game->state = GAME_STATE_PLAYING;
        }
        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (button_is_clicked(&game->pause_buttons[0],
                                  game->mouse_x, game->mouse_y))
                game->state = GAME_STATE_PLAYING;
            if (button_is_clicked(&game->pause_buttons[1],
                                  game->mouse_x, game->mouse_y)) {
                game->state = GAME_STATE_MENU;
            }
        }
        break;

    case GAME_STATE_SETTINGS:
        button_update_hover(&game->settings_back_button,
                            game->mouse_x, game->mouse_y);
        if (event->type == SDL_EVENT_KEY_DOWN) {
            switch (event->key.key) {
            case SDLK_ESCAPE:
                game->state = GAME_STATE_MENU;
                break;
            case SDLK_UP:
                if (game->settings_focus > 0) game->settings_focus--;
                break;
            case SDLK_DOWN:
                if (game->settings_focus < 1) game->settings_focus++;
                break;
            case SDLK_LEFT:
                if (game->settings_focus == 0) {
                    slider_set_value(&game->settings_volume_slider,
                        game->settings_volume_slider.value - 5.0f);
                    game->volume = game->settings_volume_slider.value;
                } else {
                    slider_set_value(&game->settings_brightness_slider,
                        game->settings_brightness_slider.value - 5.0f);
                    game->brightness = game->settings_brightness_slider.value;
                }
                break;
            case SDLK_RIGHT:
                if (game->settings_focus == 0) {
                    slider_set_value(&game->settings_volume_slider,
                        game->settings_volume_slider.value + 5.0f);
                    game->volume = game->settings_volume_slider.value;
                } else {
                    slider_set_value(&game->settings_brightness_slider,
                        game->settings_brightness_slider.value + 5.0f);
                    game->brightness = game->settings_brightness_slider.value;
                }
                break;
            case SDLK_RETURN:
                game->state = GAME_STATE_MENU;
                break;
            default: break;
            }
        }
        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (button_is_clicked(&game->settings_back_button,
                                  game->mouse_x, game->mouse_y)) {
                game->state = GAME_STATE_MENU;
            }
            if (slider_handle_click(&game->settings_volume_slider,
                                    game->mouse_x, game->mouse_y)) {
                game->volume = game->settings_volume_slider.value;
                game->settings_focus = 0;
            }
            if (slider_handle_click(&game->settings_brightness_slider,
                                    game->mouse_x, game->mouse_y)) {
                game->brightness = game->settings_brightness_slider.value;
                game->settings_focus = 1;
            }
        }
        break;

    default: break;
    }
}

/* ── Per-frame update ──────────────────────────────────────────────────── */

void game_update(Game *game)
{
    if (!game) return;

    Uint64 now      = SDL_GetTicks();
    float  dt       = (float)(now - game->last_ticks) / 1000.0f;
    if (dt > 0.1f) dt = 0.1f;
    game->delta_time = dt;
    game->last_ticks = now;

    if (game->state == GAME_STATE_PLAYING &&
        game->player && game->world) {

        Player   *p   = game->player;
        Location *loc = world_get_location(game->world,
                                           p->current_location_id);

        /* ── Movement input ── */
        p->vx = 0.0f; p->vy = 0.0f;
        p->is_moving = 0;
        p->is_moving_backwards = 0;

        if (game->keys[SDL_SCANCODE_A] || game->keys[SDL_SCANCODE_LEFT]) {
            p->vx = -PLAYER_SPEED; p->facing_right = 0;
        }
        else if (game->keys[SDL_SCANCODE_D] || game->keys[SDL_SCANCODE_RIGHT]) {
            p->vx = +PLAYER_SPEED; p->facing_right = 1;
        }
        else if (game->keys[SDL_SCANCODE_W] || game->keys[SDL_SCANCODE_UP]) {
            p->vy = -PLAYER_SPEED;  // CHANGED: Remove the * 0.3f
        }
        else if (game->keys[SDL_SCANCODE_S] || game->keys[SDL_SCANCODE_DOWN]) {
            p->vy = +PLAYER_SPEED;  // CHANGED: Remove the * 0.3f
        }

        /* Set is_moving if any velocity is applied */
        p->is_moving = (p->vx != 0.0f || p->vy != 0.0f) ? 1 : 0;
        
        /* ── Apply movement ── */
        p->x += p->vx * dt;
        p->y += p->vy * dt;

        float half_w = (float)PLAYER_W / 2.0f;

        /* ── Collision with room colliders (using custom collider) ── */
        if (loc) {
            Rect pr = {
                p->x - half_w + p->collider_offset_x,
                p->y - (float)PLAYER_SPRITE_H + p->collider_offset_y,
                (float)p->collider_w,
                (float)p->collider_h
            };
            for (int i = 0; i < loc->collider_count; i++)
                rect_resolve_collision(&pr, &loc->colliders[i], &p->vx, &p->vy);
            p->x = pr.x - p->collider_offset_x + half_w;
            p->y = pr.y - p->collider_offset_y + (float)PLAYER_SPRITE_H;
        }
        /* ── Check triggers ── */
        game->near_interactive       = 0;
        game->interactive_trigger_id = -1;

        if (loc) {
            /* Full sprite rect — used for interactive-proximity checks */
            Rect pr = {
                p->x - half_w,
                p->y - (float)PLAYER_SPRITE_H,
                (float)PLAYER_W, (float)PLAYER_SPRITE_H
            };
            /* Feet-only rect — used for room-transition triggers so the door
               fires when the player's feet (bottom edge) enter the zone */
            Rect feet = {
                p->x - half_w,
                p->y - 4.0f,
                (float)PLAYER_W, 4.0f
            };
            for (int i = 0; i < loc->trigger_count; i++) {
                TriggerZone *tz = &loc->triggers[i];

                if (tz->target_location_id >= 0) {
                    /* Room transition: only trigger when feet enter the zone */
                    if (rect_overlaps(&feet, &tz->bounds)) {
                        game_change_location(game,
                            tz->target_location_id,
                            tz->spawn_x, tz->spawn_y);
                        break;
                    }
                } else {
                    /* Interactive: detect proximity */
                    Rect near_zone = {
                        tz->bounds.x - 40.0f, tz->bounds.y - 20.0f,
                        tz->bounds.w + 80.0f, tz->bounds.h + 40.0f
                    };
                    if (rect_overlaps(&pr, &near_zone)) {
                        game->near_interactive       = 1;
                        game->interactive_trigger_id = tz->trigger_id;
                        const char *label;
                        if (tz->trigger_id == 60)
                            label = "Press [E] to enter locker";
                        else if (tz->trigger_id == 75)
                            label = "Press [E] to interact";
                        else if (tz->trigger_id == 91)
                            label = "Press [E] to examine";
                        else if (tz->trigger_id == 92)
                            label = "Press [E] to examine";
                        else
                            label = "Press E to talk";
                        strncpy(game->interact_label, label,
                                sizeof(game->interact_label) - 1);
                        game->interact_label[sizeof(game->interact_label) - 1] = '\0';
                        break;
                    }
                }
            }
        }

        /* ── Animate player ── */
        player_update(p, dt);

        /* ── Camera follow ── */
        camera_follow(&game->camera, p->x, p->y, dt);

        /* ── Update NPCs that belong to the current location ── */
        if (game->npc_manager) {
            for (int i = 0; i < game->npc_manager->npc_count; i++) {
                NPC *n = &game->npc_manager->npcs[i];
                if (n->location_id == p->current_location_id)
                    npc_update(n, dt);
            }
        }

    } else if (game->state == GAME_STATE_DIALOGUE && game->player) {
        dialogue_state_update(&game->dialogue_state, dt);
    }

    /* Tick the pickup notification regardless of game state */
    if (game->pickup_notify_timer > 0.0f) {
        game->pickup_notify_timer -= dt;
        if (game->pickup_notify_timer < 0.0f)
            game->pickup_notify_timer = 0.0f;
    }
}

/* ── Rendering ──────────────────────────────────────────────────────────── */

void game_render(Game *game)
{
    if (!game) return;
    switch (game->state) {
    case GAME_STATE_MENU:      game_render_menu(game);      break;
    case GAME_STATE_SETTINGS:  game_render_settings(game);  break;
    case GAME_STATE_PLAYING:   game_render_playing(game);   break;
    case GAME_STATE_DIALOGUE:
        game_render_playing(game);
        game_render_dialogue_overlay(game);
        break;
    case GAME_STATE_INVENTORY: game_render_inventory(game); break;
    case GAME_STATE_LOCKER:    game_render_locker(game);    break;
    case GAME_STATE_PAUSE:
        game_render_playing(game);
        game_render_pause(game);
        break;
    default: break;
    }

    /* Apply brightness: draw a black overlay when brightness < 100
     * (skipped in settings so sliders remain visible at all brightness levels)
     * Alpha is capped at 220 rather than 255 so some scene detail is still
     * visible even at the minimum brightness setting. */
    if (game->brightness < 100.0f && game->state != GAME_STATE_SETTINGS) {
        Uint8 alpha = (Uint8)((1.0f - game->brightness / 100.0f) * 220.0f);
        render_filled_rect(game->renderer, 0, 0, WINDOW_W, WINDOW_H,
                           0, 0, 0, alpha);
    }
}

/* ── Menu ────────────────────────────────────────────────────────────────── */

void game_render_menu(Game *game)
{
    SDL_Renderer *r = game->renderer;

    if (game->state == GAME_STATE_MENU) {
        if (game->title_screen_texture) {
            // Draw full-screen PNG
            render_texture(r, game->title_screen_texture,
                          0, 0, WINDOW_W, WINDOW_H);
        } else {
            // Fallback to colored background if PNG fails to load
            render_filled_rect(r, 0, 0, WINDOW_W, WINDOW_H,
                              0, 0, 0, 255);
        }
        // Then draw your menu buttons on top
        // ... existing menu button code ...
    }

    for (int i = 0; i < 3; i++) {
        Button btn = game->buttons[i];
        if (i == game->current_menu_choice) btn.is_hovered = 1;
        draw_button_menu(r, &btn);
    }
    /* mouse coordinates */
    render_text_centered(r,
        "WASD: move | E: interact | I: inventory | ESC: pause",
        WINDOW_W/2, WINDOW_H - 28, 1, 78, 25, 25);
    char buf[64];
    snprintf(buf, sizeof(buf), "mouse: %d, %d",
             (int)game->mouse_x, (int)game->mouse_y);

    /* top-left debug text */
    render_text(r, buf, 10, 10, 2, 255, 255, 255);
}

/* ── Playing ─────────────────────────────────────────────────────────────── */


/* Render the flashlight cone when the flashlight is active.
 * Casts FL_NUM_RAYS rays within a 90-degree cone (±45°, i.e. π/4 radians,
 * from the player's facing direction) and draws the lit area as a
 * triangle fan using additive blending. */
#define FL_NUM_RAYS  48
#define FL_MAX_DIST  250.0f
#define FL_HALF_CONE (M_PI / 4.0)   /* half-cone: 45° (π/4 radians) each side */

static void render_flashlight_beam(Game *game)
{
    if (!game->flashlight_active) return;
    Player   *p   = game->player;
    Location *loc = world_get_location(game->world, p->current_location_id);
    if (!loc) return;

    /* World-space origin: vertical centre of the player collider */
    float ox = p->x;
    float oy = p->y - (float)PLAYER_COLLIDER_OFFSET_Y
               + (float)PLAYER_COLLIDER_H * 0.5f;

    /* Base angle from the player's facing direction */
    double base_angle;
    switch (p->current_direction) {
    case DIRECTION_EAST:  base_angle =  0.0;          break;
    case DIRECTION_WEST:  base_angle =  M_PI;         break;
    case DIRECTION_NORTH: base_angle = -M_PI / 2.0;  break;
    case DIRECTION_SOUTH: base_angle =  M_PI / 2.0;  break;
    default:              base_angle =  0.0;          break;
    }

    /* Build a triangle-fan in screen space:
     *   verts[0]          = ray origin (player centre)
     *   verts[1..N+1]     = hit points for each ray */
    SDL_Vertex verts[FL_NUM_RAYS + 2];
    int        indices[FL_NUM_RAYS * 3];

    int sx0 = camera_to_screen_x(&game->camera, ox);
    int sy0 = camera_to_screen_y(&game->camera, oy);
    verts[0].position.x  = (float)sx0;
    verts[0].position.y  = (float)sy0;
    verts[0].color.r     = 1.0f;
    verts[0].color.g     = 1.0f;
    verts[0].color.b     = 0.8f;
    verts[0].color.a     = 0.45f;
    verts[0].tex_coord.x = 0.0f;
    verts[0].tex_coord.y = 0.0f;

    for (int i = 0; i <= FL_NUM_RAYS; i++) {
        double angle = base_angle - FL_HALF_CONE
                       + (2.0 * FL_HALF_CONE * i) / FL_NUM_RAYS;
        float dx    = (float)cos(angle);
        float dy    = (float)sin(angle);
        float hx    = ox + dx * FL_MAX_DIST;
        float hy    = oy + dy * FL_MAX_DIST;

        int sx = camera_to_screen_x(&game->camera, hx);
        int sy = camera_to_screen_y(&game->camera, hy);

        /* Fade alpha toward the cone edges: 1.0 at centre, 0.0 at ±45°.
         * Formula: edge = 1 - |2*(i/N) - 1|, mapping [0,N] → [0,1,0]. */
        float edge = 1.0f - 2.0f * fabsf((float)i / FL_NUM_RAYS - 0.5f);

        verts[i + 1].position.x  = (float)sx;
        verts[i + 1].position.y  = (float)sy;
        verts[i + 1].color.r     = 1.0f;
        verts[i + 1].color.g     = 1.0f;
        verts[i + 1].color.b     = 0.7f;
        verts[i + 1].color.a     = 0.20f * edge;
        verts[i + 1].tex_coord.x = 0.0f;
        verts[i + 1].tex_coord.y = 0.0f;
    }

    for (int i = 0; i < FL_NUM_RAYS; i++) {
        indices[i * 3 + 0] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = i + 2;
    }

    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_ADD);
    SDL_RenderGeometry(game->renderer, NULL,
                       verts, FL_NUM_RAYS + 2,
                       indices, FL_NUM_RAYS * 3);
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
}


/* Number of triangle-fan segments for the ambient glow circle. */
#define DARK_AMBIENT_NUM_SEGS  48
/* Screen-space radius (pixels) of the ambient circle around the player. */
#define DARK_AMBIENT_RADIUS    120

/* Renders the archive room darkness effect (location 0 only).
 *
 * Builds a light-mask on the dark_overlay render-target:
 *   – Starts fully black (opaque).
 *   – Adds a small, very dim ambient circle around the player so nearby
 *     shapes are faintly visible.
 *   – If the flashlight is active, adds a bright white triangle fan in the
 *     player's facing direction (same raycast geometry as the beam).
 *
 * The finished mask is then composited onto the screen with
 * SDL_BLENDMODE_MOD, which multiplies every screen pixel by the mask colour:
 *   black mask  → screen pixel becomes black (hidden)
 *   white mask  → screen pixel is unchanged (fully revealed)
 *
 * The additive flashlight beam drawn afterwards by render_flashlight_beam()
 * adds the warm yellowish glow on top of the revealed scene.
 */
static void render_archive_darkness(Game *game)
{
    if (!game->dark_overlay) return;
    if (game->player->current_location_id != 0) return;

    SDL_Renderer *r   = game->renderer;
    Player       *p   = game->player;
    Location     *loc = world_get_location(game->world, p->current_location_id);

    /* World-space origin: vertical centre of the player collider */
    float ox = p->x;
    float oy = p->y - (float)PLAYER_COLLIDER_OFFSET_Y
                    + (float)PLAYER_COLLIDER_H * 0.5f;
    int sx0 = camera_to_screen_x(&game->camera, ox);
    int sy0 = camera_to_screen_y(&game->camera, oy);

    /* ── Build the light mask ──────────────────────────────────────────── */
    SDL_SetRenderTarget(r, game->dark_overlay);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);

    /* Ambient glow: a dim warm circle centred on the player.
     * Centre brightness 0.45 – nearby shapes are visible but colours are
     * heavily muted.  Falls off to black at the edge so vision is clearly
     * limited to a small radius. */
    {
        SDL_Vertex av[DARK_AMBIENT_NUM_SEGS + 2];
        int        ai[DARK_AMBIENT_NUM_SEGS * 3];

        av[0].position.x  = (float)sx0;
        av[0].position.y  = (float)sy0;
        av[0].color.r     = 0.45f;
        av[0].color.g     = 0.42f;
        av[0].color.b     = 0.35f;
        av[0].color.a     = 1.0f;
        av[0].tex_coord.x = 0.0f;
        av[0].tex_coord.y = 0.0f;

        for (int i = 0; i <= DARK_AMBIENT_NUM_SEGS; i++) {
            double angle = (2.0 * M_PI * i) / DARK_AMBIENT_NUM_SEGS;
            av[i + 1].position.x  = (float)sx0
                                    + DARK_AMBIENT_RADIUS * (float)cos(angle);
            av[i + 1].position.y  = (float)sy0
                                    + DARK_AMBIENT_RADIUS * (float)sin(angle);
            av[i + 1].color.r     = 0.0f;
            av[i + 1].color.g     = 0.0f;
            av[i + 1].color.b     = 0.0f;
            av[i + 1].color.a     = 1.0f;
            av[i + 1].tex_coord.x = 0.0f;
            av[i + 1].tex_coord.y = 0.0f;
        }
        for (int i = 0; i < DARK_AMBIENT_NUM_SEGS; i++) {
            ai[i * 3 + 0] = 0;
            ai[i * 3 + 1] = i + 1;
            ai[i * 3 + 2] = i + 2;
        }
        SDL_RenderGeometry(r, NULL,
                           av, DARK_AMBIENT_NUM_SEGS + 2,
                           ai, DARK_AMBIENT_NUM_SEGS * 3);
    }

    /* Flashlight cone: reveal the scene in the illuminated area.
     * Uses the same raycasting geometry as render_flashlight_beam() but
     * draws in bright white so MOD blend fully uncovers the room texture. */
    if (game->flashlight_active && loc) {
        double base_angle;
        switch (p->current_direction) {
        case DIRECTION_EAST:  base_angle =  0.0;         break;
        case DIRECTION_WEST:  base_angle =  M_PI;        break;
        case DIRECTION_NORTH: base_angle = -M_PI / 2.0;  break;
        case DIRECTION_SOUTH: base_angle =  M_PI / 2.0;  break;
        default:              base_angle =  0.0;         break;
        }

        SDL_Vertex fv[FL_NUM_RAYS + 2];
        int        fi[FL_NUM_RAYS * 3];

        fv[0].position.x  = (float)sx0;
        fv[0].position.y  = (float)sy0;
        fv[0].color.r     = 1.0f;
        fv[0].color.g     = 1.0f;
        fv[0].color.b     = 1.0f;
        fv[0].color.a     = 1.0f;
        fv[0].tex_coord.x = 0.0f;
        fv[0].tex_coord.y = 0.0f;

        for (int i = 0; i <= FL_NUM_RAYS; i++) {
            double angle = base_angle - FL_HALF_CONE
                           + (2.0 * FL_HALF_CONE * i) / FL_NUM_RAYS;
            float dx   = (float)cos(angle);
            float dy   = (float)sin(angle);
            float hx   = ox + dx * FL_MAX_DIST;
            float hy   = oy + dy * FL_MAX_DIST;

            float edge = 1.0f - 2.0f * fabsf((float)i / FL_NUM_RAYS - 0.5f);

            fv[i + 1].position.x  = (float)camera_to_screen_x(&game->camera, hx);
            fv[i + 1].position.y  = (float)camera_to_screen_y(&game->camera, hy);
            fv[i + 1].color.r     = 1.0f;
            fv[i + 1].color.g     = 1.0f;
            fv[i + 1].color.b     = 0.9f;
            fv[i + 1].color.a     = 0.55f * edge;
            fv[i + 1].tex_coord.x = 0.0f;
            fv[i + 1].tex_coord.y = 0.0f;
        }
        for (int i = 0; i < FL_NUM_RAYS; i++) {
            fi[i * 3 + 0] = 0;
            fi[i * 3 + 1] = i + 1;
            fi[i * 3 + 2] = i + 2;
        }
        SDL_RenderGeometry(r, NULL,
                           fv, FL_NUM_RAYS + 2,
                           fi, FL_NUM_RAYS * 3);
    }

    /* ── Apply the mask to the screen via multiply blend ──────────────── */
    SDL_SetRenderTarget(r, NULL);
    SDL_SetTextureBlendMode(game->dark_overlay, SDL_BLENDMODE_MOD);
    SDL_FRect dst = { 0.0f, 0.0f, (float)WINDOW_W, (float)WINDOW_H };
    SDL_RenderTexture(r, game->dark_overlay, NULL, &dst);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
}


void game_render_playing(Game *game)
{
    if (!game || !game->player || !game->world) return;

    Location *loc = world_get_location(game->world,
                                       game->player->current_location_id);
    if (loc) world_render_room(loc, game->renderer, &game->camera);

    /* Player */
    int sx = camera_to_screen_x(&game->camera, game->player->x)
             - PLAYER_W / 2;
    int sy = camera_to_screen_y(&game->camera, game->player->y)
             - PLAYER_SPRITE_H;
    player_render(game->player, game->renderer, sx, sy);

    /* Render NPCs that belong to the current location */
    if (game->npc_manager) {
        int loc_id = game->player->current_location_id;
        for (int i = 0; i < game->npc_manager->npc_count; i++) {
            NPC *n = &game->npc_manager->npcs[i];
            if (n->location_id == loc_id)
                npc_render(n, game->renderer, &game->camera);
        }
    }

    /* Archive room darkness: MOD-blend light mask (only in location 0).
     * Must run after the scene is drawn but before the additive beam. */
    render_archive_darkness(game);

    /* Flashlight beam (additive warm glow, rendered on top of the darkness) */
    render_flashlight_beam(game);

    /* Stranger NPC: draw a bright yellow exclamation mark above its head
     * when in the Entrance Hall (location 0) so the player can spot it.
     * The body/head are already drawn by world_render_room() via decor,
     * so we only add the visual indicator here. */
    if (game->player->current_location_id == 0) {
        /* Centre of the 30-px body, above the 44-px head (FLOOR_Y-150). */
        int npc_sx = camera_to_screen_x(&game->camera,
                                        STRANGER_NPC_X + 15);  /* half body width */
        int npc_sy = camera_to_screen_y(&game->camera, FLOOR_Y - 190);
        /* Bright yellow "!" above the NPC head */
    }

    /* Interaction prompt */
    if (game->near_interactive) {
        int px = camera_to_screen_x(&game->camera, game->player->x);
        int py = camera_to_screen_y(&game->camera,
                                    game->player->y - PLAYER_SPRITE_H - 8);
        ui_draw_interact_prompt(game->renderer, game->interact_label, px, py);
    }

    /* HUD */
    ui_draw_hud(game->renderer, game->player);
    /* Player coordinates (debug) */
       {
           char coord_buf[32];
           snprintf(coord_buf, sizeof(coord_buf),
                    "X:%.0f  Y:%.0f", game->player->x, game->player->y);
           render_text(game->renderer, coord_buf, 14, 26, 1, 110, 70, 70);
       }
    /* Objective bar */
    if (game->story && game->player) {
        const char *obj = story_get_objective(game->player);
        ui_draw_objective_bar(game->renderer, obj);
    }

    /* Chapter label */
    if (game->story)
        render_text(game->renderer, "Prologue",
                    WINDOW_W - 100, 12, 1, 110, 30, 30);

    render_text(game->renderer, "I:inv  ESC:pause",
                WINDOW_W - 136, WINDOW_H - 18, 1, 66, 18, 18);

    /* ── Item pickup notification ── */
    if (game->pickup_notify_timer > 0.0f) {
        /* Fade in during the first 0.3 s, full for 1.5 s, fade out for 0.7 s */
        float t = game->pickup_notify_timer; /* remaining time */
        float alpha_f;
        if (t > 2.2f)        alpha_f = (2.5f - t) / 0.3f;   /* fade in  */
        else if (t > 0.7f)   alpha_f = 1.0f;                 /* full     */
        else                 alpha_f = t / 0.7f;             /* fade out */
        if (alpha_f > 1.0f) alpha_f = 1.0f;
        Uint8 a = (Uint8)(alpha_f * 255.0f);

        /* Build label */
        char notify_buf[80];
        snprintf(notify_buf, sizeof(notify_buf),
                 "Obtained: %s", game->pickup_item_name);

        /* Measure text width (8px per char × scale 2) */
        int text_w = (int)(strlen(notify_buf) * 8 * 2);
        int text_h = 16;  /* 8px × scale 2 */
        int bx = (WINDOW_W - text_w) / 2 - 14;
        int by = 48;
        int bw = text_w + 28;
        int bh = text_h + 18;

        render_filled_rect(game->renderer, bx, by, bw, bh, 10, 3, 3,
                           (Uint8)(a * 0.82f));
        render_rect_outline(game->renderer, bx, by, bw, bh, 160, 60, 60, a);
        render_text(game->renderer, notify_buf,
                    bx + 14, by + 9, 2,
                    (Uint8)(220 * alpha_f),
                    (Uint8)(140 * alpha_f),
                    (Uint8)(140 * alpha_f));
    }
}

/* ── Dialogue overlay ────────────────────────────────────────────────────── */

void game_render_dialogue_overlay(Game *game)
{
    if (!game) return;
    dialogue_render(&game->dialogue_state, game->renderer,
                    WINDOW_W, WINDOW_H);
}

/* ── Inventory ───────────────────────────────────────────────────────────── */

static SDL_Texture *get_item_icon_texture(const Game *game, int item_id)
{
    if (!game) return NULL;
    switch (item_id) {
    case ITEM_ID_FLASHLIGHT: return game->item_flashlight_texture;
    case ITEM_ID_GASMASK:    return game->item_gasmask_texture;
    default:                 return NULL;
    }
}

void game_render_inventory(Game *game)
{
    if (!game || !game->player) return;
    SDL_Renderer *r = game->renderer;

    /* Render the scene behind the panel */
    if (game->world) {
        Location *loc = world_get_location(game->world,
                                           game->player->current_location_id);
        if (loc) world_render_room(loc, game->renderer, &game->camera);
    }

    render_filled_rect(r, 0, 0, WINDOW_W, WINDOW_H, 0, 0, 0, 170);

    int px = 140, py = 60, pw = WINDOW_W - 280, ph = WINDOW_H - 120;

    /* Panel background */
    render_filled_rect(r, px, py, pw, ph, 25, 8, 8, 235);
    render_rect_outline(r, px, py, pw, ph, 110, 25, 25, 255);
    render_rect_outline(r, px+2, py+2, pw-4, ph-4, 55, 15, 15, 180);

    render_text_centered(r, "INVENTORY", WINDOW_W/2, py + 14, 2, 200, 110, 110);
    render_filled_rect(r, px+18, py+42, pw-36, 2, 70, 18, 18, 190);

    /* ── Grid layout constants ── */
#define INV_COLS       4
#define INV_CELL_SIZE  84   /* square cell size in px          */
#define INV_CELL_GAP   10   /* gap between cells               */
#define INV_ICON_PAD    8   /* padding inside cell to icon box */

    /* Centre the 4-column grid in the left ~55% of the panel */
    int grid_total_w = INV_COLS * INV_CELL_SIZE + (INV_COLS - 1) * INV_CELL_GAP;
    int grid_x = px + 24;
    int grid_y = py + 90;

    /* Detail pane sits to the right of the grid */
    int detail_x = grid_x + grid_total_w + 24;
    int detail_w = (px + pw - 18) - detail_x;  /* reaches the right border */

    Player *p = game->player;

    /* Draw all grid cells (filled slots and empty slots) */
    for (int i = 0; i < INVENTORY_CAPACITY; i++) {
        int col = i % INV_COLS;
        int row = i / INV_COLS;
        int cx = grid_x + col * (INV_CELL_SIZE + INV_CELL_GAP);
        int cy = grid_y + row * (INV_CELL_SIZE + INV_CELL_GAP);
        int is_sel  = (i == game->selected_inventory_slot);
        int has_item = (i < p->inventory_count);

        /* Cell background — bright red */
        render_filled_rect(r, cx, cy, INV_CELL_SIZE, INV_CELL_SIZE,
                           has_item ? (is_sel ? 180 : 120) : 60,
                           has_item ? (is_sel ?  20 :  10) :  8,
                           has_item ? (is_sel ?  20 :  10) :  8, 220);
        render_rect_outline(r, cx, cy, INV_CELL_SIZE, INV_CELL_SIZE,
                            is_sel ? 255 : 180,
                            is_sel ?  50 :  20,
                            is_sel ?  50 :  20, 220);

        /* Selection highlight */
        if (is_sel) {
            render_filled_rect(r, cx, cy, INV_CELL_SIZE, INV_CELL_SIZE,
                               255, 60, 60, 60);
            render_rect_outline(r, cx, cy, INV_CELL_SIZE, INV_CELL_SIZE,
                                255, 80, 80, 240);
        }

        if (has_item) {
            /* Icon box inside the cell */
            int icon_size = INV_CELL_SIZE - INV_ICON_PAD * 2 - 20;
            int ix = cx + INV_ICON_PAD;
            int icon_y = cy + INV_ICON_PAD;
            render_filled_rect(r, ix, icon_y, icon_size, icon_size,
                               120, 25, 25, 220);
            render_rect_outline(r, ix, icon_y, icon_size, icon_size,
                                155, 35, 35, 200);

            /* Item icon texture (flashlight / gasmask) */
            SDL_Texture *item_tex = get_item_icon_texture(game, p->inventory[i].id);
            if (item_tex)
                render_texture(r, item_tex, ix, icon_y, icon_size, icon_size);

            /* Item name (1-2 words, clipped) below icon */
            char short_name[18];
            strncpy(short_name, p->inventory[i].name, sizeof(short_name) - 1);
            short_name[sizeof(short_name) - 1] = '\0';
            /* Truncate with ".." if too wide (max ~9 chars at scale 1) */
            if (strlen(short_name) > 9) {
                short_name[7] = '.';
                short_name[8] = '.';
                short_name[9] = '\0';
            }
            render_text(r, short_name,
                        cx + 4, cy + INV_CELL_SIZE - 14, 1,
                        is_sel ? 255 : 160,
                        is_sel ? 180 : 90,
                        is_sel ? 180 : 90);
        }
    }

    /* ── Detail pane for selected item ── */
    int sel = game->selected_inventory_slot;
    if (sel >= 0 && sel < p->inventory_count) {
        const Item *it = &p->inventory[sel];

        /* Detail box background */
        render_filled_rect(r, detail_x, grid_y, detail_w,
                           4 * (INV_CELL_SIZE + INV_CELL_GAP) - INV_CELL_GAP,
                           20, 6, 6, 200);
        render_rect_outline(r, detail_x, grid_y, detail_w,
                            4 * (INV_CELL_SIZE + INV_CELL_GAP) - INV_CELL_GAP,
                            90, 22, 22, 200);

        /* Large icon placeholder */
        int big_icon = 64;
        int bix = detail_x + (detail_w - big_icon) / 2;
        render_filled_rect(r, bix, grid_y + 14, big_icon, big_icon,
                           130, 28, 28, 240);
        render_rect_outline(r, bix, grid_y + 14, big_icon, big_icon,
                            170, 40, 40, 220);

        /* Item icon texture in detail pane */
        {
            SDL_Texture *item_tex = get_item_icon_texture(game, it->id);
            if (item_tex)
                render_texture(r, item_tex, bix, grid_y + 14, big_icon, big_icon);
        }

        /* Item name */
        render_text_centered(r, it->name,
                             detail_x + detail_w / 2,
                             grid_y + 14 + big_icon + 12, 2,
                             230, 160, 160);

        /* Divider */
        render_filled_rect(r, detail_x + 8,
                           grid_y + 14 + big_icon + 32,
                           detail_w - 16, 1, 80, 20, 20, 180);

        /* Description (wrapped) */
        render_text_wrapped(r, it->description,
                            detail_x + 10,
                            grid_y + 14 + big_icon + 40,
                            detail_w - 20, 1, 14,
                            180, 100, 100);

        /* "Usable" hint */
        if (it->usable)
            render_text_centered(r, "[U] Use item",
                                 detail_x + detail_w / 2,
                                 grid_y + 4 * (INV_CELL_SIZE + INV_CELL_GAP) - 30,
                                 1, 130, 200, 130);
    } else {
        /* Empty selection — show placeholder */
        render_filled_rect(r, detail_x, grid_y, detail_w,
                           4 * (INV_CELL_SIZE + INV_CELL_GAP) - INV_CELL_GAP,
                           15, 5, 5, 160);
        render_rect_outline(r, detail_x, grid_y, detail_w,
                            4 * (INV_CELL_SIZE + INV_CELL_GAP) - INV_CELL_GAP,
                            60, 15, 15, 160);
        render_text_centered(r, "No item selected",
                             detail_x + detail_w / 2,
                             grid_y + (4 * (INV_CELL_SIZE + INV_CELL_GAP)) / 2,
                             1, 70, 20, 20);
    }

    /* Item count label */
    {
        char count_buf[32];
        snprintf(count_buf, sizeof(count_buf),
                 "Items: %d / %d", p->inventory_count, INVENTORY_CAPACITY);
        render_text(r, count_buf, px + 24, py + ph - 22, 1, 100, 30, 30);
    }

    render_text_centered(r, "[Arrow keys] navigate   [I] or [ESC] close",
                         WINDOW_W/2, py + ph - 22, 1, 78, 22, 22);
}

#undef INV_COLS
#undef INV_CELL_SIZE
#undef INV_CELL_GAP
#undef INV_ICON_PAD

/* ── Pause ───────────────────────────────────────────────────────────────── */

void game_render_pause(Game *game)
{
    if (!game) return;
    SDL_Renderer *r = game->renderer;

    render_filled_rect(r, 0, 0, WINDOW_W, WINDOW_H, 0, 0, 0, 145);

    int pw = 320, ph = 200;
    int qx = (WINDOW_W - pw) / 2, qy = (WINDOW_H - ph) / 2;

    render_filled_rect(r, qx, qy, pw, ph, 25, 8, 8, 235);
    render_rect_outline(r, qx, qy, pw, ph, 110, 25, 25, 255);
    render_text_centered(r, "PAUSED", WINDOW_W/2, qy+18, 2, 200,110,110);
    render_filled_rect(r, qx+18, qy+44, pw-36, 2, 70,18,18, 190);

    for (int i = 0; i < 2; i++) draw_button(r, &game->pause_buttons[i]);
    render_text_centered(r, "[ESC] to resume",
                         WINDOW_W/2, qy+ph-20, 1, 78,22,22);
}

/* ── Settings ────────────────────────────────────────────────────────────── */

void game_render_settings(Game *game)
{
    if (!game) return;
    SDL_Renderer *r = game->renderer;

    /* Background: reuse title screen or solid black fallback */
    if (game->title_screen_texture) {
        render_texture(r, game->title_screen_texture, 0, 0, WINDOW_W, WINDOW_H);
    } else {
        render_filled_rect(r, 0, 0, WINDOW_W, WINDOW_H, 0, 0, 0, 255);
    }
    render_filled_rect(r, 0, 0, WINDOW_W, WINDOW_H, 0, 0, 0, 160);

    /* Panel */
    int pw = 680, ph = 340;
    int px = (WINDOW_W - pw) / 2, py = 170;
    render_filled_rect(r, px, py, pw, ph, 25, 8, 8, 230);
    render_rect_outline(r, px, py, pw, ph, 110, 25, 25, 255);
    render_rect_outline(r, px+2, py+2, pw-4, ph-4, 55, 15, 15, 180);

    /* Title */
    render_text_centered(r, "SETTINGS", WINDOW_W/2, py + 20, 3, 200, 110, 110);
    render_filled_rect(r, px+18, py+54, pw-36, 2, 70, 18, 18, 190);

    /* Update focused state on sliders before rendering */
    game->settings_volume_slider.focused     = (game->settings_focus == 0);
    game->settings_brightness_slider.focused = (game->settings_focus == 1);

    /* Sliders */
    slider_render(r, &game->settings_volume_slider,     "Volume");
    slider_render(r, &game->settings_brightness_slider, "Brightness");

    /* Back button */
    draw_button_menu(r, &game->settings_back_button);

    /* Instructions */
    render_text_centered(r,
        "UP/DOWN: select  |  LEFT/RIGHT: adjust  |  ESC: back",
        WINDOW_W/2, WINDOW_H - 28, 1, 78, 25, 25);
}

/* ── Locker view ─────────────────────────────────────────────────────────── */

void game_render_locker(Game *game)
{
    if (!game) return;
    SDL_Renderer *r = game->renderer;

    /* Only one of these flags is ever set at a time; note takes priority. */
    SDL_Texture *tex = game->show_note_locker   ? game->note_locker_texture  :
                       game->show_monitor_zoom  ? game->monitor_zoom_texture :
                                                  game->locker_texture;

    if (tex) {
        render_texture(r, tex, 0, 0, WINDOW_W, WINDOW_H);
    } else {
        render_filled_rect(r, 0, 0, WINDOW_W, WINDOW_H, 0, 0, 0, 255);
    }

    if (game->show_monitor_zoom) {
        /* Hover indicator on the clickable monitor panel rect */
        if (!game->passcode_active) {
            float mx = game->mouse_x, my = game->mouse_y;
            int hovering = (mx >= MONITOR_PANEL_X &&
                            mx <= MONITOR_PANEL_X + MONITOR_PANEL_W &&
                            my >= MONITOR_PANEL_Y &&
                            my <= MONITOR_PANEL_Y + MONITOR_PANEL_H);
            /* Always show a subtle outline so the player knows it's there */
            render_rect_outline(r,
                MONITOR_PANEL_X, MONITOR_PANEL_Y,
                MONITOR_PANEL_W, MONITOR_PANEL_H,
                0, hovering ? 255 : 160, hovering ? 80 : 0,
                hovering ? 255 : 140);
            if (hovering) {
                render_text_centered(r, "Click to enter code",
                    MONITOR_PANEL_X + MONITOR_PANEL_W / 2,
                    MONITOR_PANEL_Y - 16, 1, 0, 220, 255);
            }
        }

        /* Text bar: "Use mouse button to select" */
        render_filled_rect(r, 0, WINDOW_H - 48, WINDOW_W, 48, 0, 0, 0, 200);
        render_text_centered(r, "Use mouse button to select",
                             WINDOW_W / 2, WINDOW_H - 36, 2, 220, 220, 220);

        /* Passcode overlay */
        if (game->passcode_active) {
            int pw = 320, ph = 180;
            int px = (WINDOW_W - pw) / 2;
            int py = (WINDOW_H - ph) / 2;
            render_filled_rect(r, px, py, pw, ph, 0, 0, 0, 210);
            render_rect_outline(r, px, py, pw, ph,
                game->passcode_correct ? 0 : 0,
                game->passcode_correct ? 255 : 180,
                game->passcode_correct ? 100 : 0, 255);

            if (game->passcode_correct) {
                render_text_centered(r, "PASSWORD CORRECT",
                                     WINDOW_W / 2, py + 50, 2, 0, 255, 100);
                render_text_centered(r, "Press any key to continue",
                                     WINDOW_W / 2, py + ph - 20, 1, 120, 180, 120);
            } else {
                render_text_centered(r, "ENTER CODE",
                                     WINDOW_W / 2, py + 16, 2, 0, 220, 0);

                /* Display entered digits, blanks shown as underscore */
                char display[PASSCODE_DISPLAY_SIZE] = "_ _ _ _";
                for (int i = 0; i < game->passcode_input_len; i++)
                    display[i * 2] = game->passcode_input[i];
                render_text_centered(r, display,
                                     WINDOW_W / 2, py + 60, 3, 0, 220, 0);

                if (game->passcode_wrong) {
                    render_text_centered(r, "passcode is wrong",
                                         WINDOW_W / 2, py + 120, 2, 220, 60, 60);
                }

                render_text_centered(r, "ESC: cancel",
                                     WINDOW_W / 2, py + ph - 20, 1, 120, 120, 120);
            }
        }
    } else {
        render_text_centered(r, "Press E or ESC to exit",
                             WINDOW_W / 2, WINDOW_H - 28, 1, 200, 200, 200);
    }
}
