#include "game.h"
#include "player.h"
#include "world.h"
#include "story.h"
#include "dialogue.h"
#include "ui.h"
#include "render.h"
#include "camera.h"
#include "collision.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
    g->buttons[0] = make_button(bx, 340.0f, bw, bh, "New Game");
    g->buttons[1] = make_button(bx, 410.0f, bw, bh, "Load Game");
    g->buttons[2] = make_button(bx, 480.0f, bw, bh, "Quit");

    g->pause_buttons[0] = make_button(bx, 310.0f, bw, bh, "Resume");
    g->pause_buttons[1] = make_button(bx, 380.0f, bw, bh, "Quit to Menu");

    g->last_ticks = SDL_GetTicks();
    g->keys       = SDL_GetKeyboardState(NULL);

    /* Load the dialogue box background image */
    dialogue_load_texture(&g->dialogue_state, renderer, "assets/dialogue.png");
    
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
    free(game);
}

/* ── State transitions ─────────────────────────────────────────────────── */

void game_start_new(Game *game)
{
    if (!game) return;

    if (game->player)        { player_destroy(game->player);       game->player = NULL; }
    if (game->world)         { world_destroy(game->world);         game->world  = NULL; }
    if (game->story)         { story_destroy(game->story);         game->story  = NULL; }
    if (game->dialogue_tree) {
        dialogue_tree_destroy(game->dialogue_tree);
        game->dialogue_tree = NULL;
    }

    game->player = player_create("Alex");
    game->world  = world_create();
    game->story  = story_create();

    story_populate_world(game->world, "assets/locations.txt");
    world_setup_rooms(game->world);

    camera_init(&game->camera, WINDOW_W, WINDOW_H, ROOM_W, ROOM_H);

    Location *start = world_get_location(game->world,
                                         game->player->current_location_id);
    if (start) {
        game->player->x = start->spawn_x;
        game->player->y = start->spawn_y;
    }
    camera_snap(&game->camera, game->player->x, game->player->y);

    game->state                   = GAME_STATE_PLAYING;
    game->near_interactive        = 0;
    game->interactive_trigger_id  = -1;
    game->selected_inventory_slot = 0;
    game->ending_type             = 0;
    game->ending_timer            = 0.0f;
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

    camera_snap(&game->camera, spawn_x, spawn_y);

    if (location_id == 4) /* Child's Room – meet Lily */
        story_trigger_event(game->story, game->player,
                            game->world, "meet_lily");
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
    if (game->dialogue_tree) {
        dialogue_tree_destroy(game->dialogue_tree);
        game->dialogue_tree = NULL;
    }
    game->state = GAME_STATE_PLAYING;
}

void game_trigger_ending(Game *game)
{
    if (!game || !game->story || !game->player) return;
    game->ending_type  = (int)story_determine_ending(game->story,
                                                      game->player);
    game->ending_timer = 0.0f;
    game->state        = GAME_STATE_ENDING;
}

/* ── Interaction handler ───────────────────────────────────────────────── */

/* Apply sanity/courage changes from the currently selected dialogue choice. */
static void apply_dialogue_choice_stats(Game *game)
{
    if (!game || !game->player || !game->dialogue_state.text_complete) return;

    const DialogueChoice *ch = dialogue_state_get_selected(
        &game->dialogue_state,
        game->player->courage, 0);
    if (!ch) return;

    player_modify_sanity (game->player, ch->sanity_delta);
    player_modify_courage(game->player, ch->courage_delta);
    if (ch->story_flag)
        game->player->flags |= (uint32_t)ch->story_flag;
}

static void handle_interaction(Game *game)
{
    if (!game || !game->player || !game->world) return;

    int loc_id = game->player->current_location_id;
    int tid    = game->interactive_trigger_id;
    Location *loc = world_get_location(game->world, loc_id);

    /* Guard: don't open duplicate dialogue */
    if (game->dialogue_tree) return;

    /* Basement Key pickup (Child's Room, trigger 10) */
    if (tid == 10 && loc_id == 4 &&
        !(game->player->flags & FLAG_KEY_OBTAINED)) {
        Item key;
        strncpy(key.name,        "Basement Key",           ITEM_NAME_MAX - 1);
        strncpy(key.description, "A heavy brass key. "
                                 "Opens the basement lock.",ITEM_DESC_MAX - 1);
        key.id     = 10;
        key.usable = 1;
        player_add_item(game->player, &key);
        game->player->flags |= FLAG_KEY_OBTAINED;
        game->dialogue_tree = dialogue_build_for_location(4);
    }
    /* Diary pickup (Library, trigger 1) */
    else if (tid == 1 && loc_id == 2) {
        if (!(game->player->flags & FLAG_FOUND_DIARY)) {
            Item diary;
            strncpy(diary.name,       "Professor's Diary",  ITEM_NAME_MAX - 1);
            strncpy(diary.description,"A water-stained diary "
                                      "filled with terrible confessions.",
                                      ITEM_DESC_MAX - 1);
            diary.id     = 1;
            diary.usable = 0;
            player_add_item(game->player, &diary);
            story_trigger_event(game->story, game->player,
                                game->world, "find_diary");
        }
        game->dialogue_tree = dialogue_build_for_location(2);
    }
    /* Basement door (Corridor, trigger 10) */
    else if (tid == 10 && loc_id == 1) {
        if (game->player->flags & FLAG_KEY_OBTAINED) {
            if (!(game->player->flags & FLAG_OPENED_BASEMENT)) {
                story_trigger_event(game->story, game->player,
                                    game->world, "open_basement");
                /* Add a walk-through exit trigger to basement at runtime */
                if (loc && loc->trigger_count < MAX_TRIGGER_ZONES) {
                    TriggerZone *tz = &loc->triggers[loc->trigger_count++];
                    tz->bounds.x = (float)(ROOM_W - 80);
                    tz->bounds.y = (float)(FLOOR_Y - 200);
                    tz->bounds.w = 80.0f;
                    tz->bounds.h = 200.0f;
                    tz->target_location_id = 3;
                    tz->trigger_id = -1;
                    tz->spawn_x = 400.0f;
                    tz->spawn_y = (float)FLOOR_Y;
                }
            }
            game->dialogue_tree = dialogue_build_for_location(1 + 100);
        } else {
            game->dialogue_tree = dialogue_build_for_location(1);
        }
    }
    /* Ritual circle (Ritual Room, trigger 20) */
    else if (tid == 20 && loc_id == 5) {
        if (!(game->player->flags & FLAG_MONSTER_AWARE)) {
            story_trigger_event(game->story, game->player,
                                game->world, "study_creature");
        }
        if (!(game->player->flags & FLAG_SOLVED_PUZZLE)) {
            story_trigger_event(game->story, game->player,
                                game->world, "solve_puzzle");
        }
        if ((game->player->flags & FLAG_FOUND_DIARY) &&
            (game->player->flags & FLAG_SOLVED_PUZZLE) &&
            !(game->player->flags & FLAG_KNOWS_TRUTH)) {
            story_trigger_event(game->story, game->player,
                                game->world, "learn_truth");
            game->player->flags |= FLAG_LILY_TRUSTS_PLAYER;
        }
        game->dialogue_tree = dialogue_build_for_location(5);
        /* Check for ending condition after ritual */
        if (game->player->flags & FLAG_KNOWS_TRUTH) {
            /* End will be triggered after dialogue */
            game->ending_type = -1; /* signal to trigger ending on dialogue end */
        }
    }
    /* Portrait interaction (Entrance Hall, trigger 30) */
    else if (tid == 30 && loc_id == 0) {
        game->dialogue_tree = dialogue_build_for_location(30);
    }
    /* Stranger NPC interaction (Entrance Hall, trigger 40) */
    else if (tid == 40 && loc_id == 0) {
        game->dialogue_tree = dialogue_build_for_location(40);
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
                if (game->current_menu_choice == 0) game_start_new(game);
                if (game->current_menu_choice == 2) game->state = GAME_STATE_QUIT;
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
                /* Apply stat changes from the chosen option before advancing */
                apply_dialogue_choice_stats(game);
                int cont = dialogue_state_advance(&game->dialogue_state,
                    game->player ? game->player->courage : 50, 0);
                if (!cont) {
                    game_end_dialogue(game);
                    /* Trigger ending if flagged */
                    if (game->ending_type == -1)
                        game_trigger_ending(game);
                }
            }
            if (event->key.key == SDLK_ESCAPE)
                game_end_dialogue(game);
        }
        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            /* Apply stat changes before advancing on mouse click too */
            apply_dialogue_choice_stats(game);
            int cont = dialogue_state_advance(&game->dialogue_state,
                game->player ? game->player->courage : 50, 0);
            if (!cont) {
                game_end_dialogue(game);
                if (game->ending_type == -1)
                    game_trigger_ending(game);
            }
        }
        break;

    case GAME_STATE_INVENTORY:
        if (event->type == SDL_EVENT_KEY_DOWN) {
            if (event->key.key == SDLK_ESCAPE ||
                event->key.key == SDLK_I)
                game->state = GAME_STATE_PLAYING;
            if (event->key.key == SDLK_UP &&
                game->selected_inventory_slot > 0)
                game->selected_inventory_slot--;
            if (event->key.key == SDLK_DOWN &&
                game->player &&
                game->selected_inventory_slot <
                    game->player->inventory_count - 1)
                game->selected_inventory_slot++;
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

    case GAME_STATE_ENDING:
        if (event->type == SDL_EVENT_KEY_DOWN ||
            event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (game->ending_timer > 3.0f)
                game->state = GAME_STATE_MENU;
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

        if (game->keys[SDL_SCANCODE_A] || game->keys[SDL_SCANCODE_LEFT]) {
            p->vx = -PLAYER_SPEED; p->facing_right = 0; p->is_moving = 1;
        }
        if (game->keys[SDL_SCANCODE_D] || game->keys[SDL_SCANCODE_RIGHT]) {
            p->vx = +PLAYER_SPEED; p->facing_right = 1; p->is_moving = 1;
        }
        if (game->keys[SDL_SCANCODE_W] || game->keys[SDL_SCANCODE_UP])
            p->vy = -PLAYER_SPEED * 0.3f;
        if (game->keys[SDL_SCANCODE_S] || game->keys[SDL_SCANCODE_DOWN])
            p->vy = +PLAYER_SPEED * 0.3f;

        /* ── Apply movement ── */
        p->x += p->vx * dt;
        p->y += p->vy * dt;

        /* ── Floor clamp (allow full vertical movement) ── */
        float floor_min = 30.0f;
        float floor_max = (float)(ROOM_H - 30);
        if (p->y < floor_min) p->y = floor_min;
        if (p->y > floor_max) p->y = floor_max;

        /* ── Room bounds ── */
        float half_w = (float)PLAYER_W / 2.0f;
        if (p->x < half_w + 42.0f) p->x = half_w + 42.0f;
        if (p->x > (float)(ROOM_W - (int)half_w - 42))
            p->x = (float)(ROOM_W - (int)half_w - 42);

        /* ── Collision with room colliders ── */
        if (loc) {
            Rect pr = {
                p->x - half_w,
                p->y - (float)PLAYER_SPRITE_H,
                (float)PLAYER_W, (float)PLAYER_SPRITE_H
            };
            for (int i = 0; i < loc->collider_count; i++)
                rect_resolve_collision(&pr, &loc->colliders[i], &p->vx, &p->vy);
            p->x = pr.x + half_w;
            p->y = pr.y + (float)PLAYER_SPRITE_H;
        }

        /* ── Check triggers ── */
        game->near_interactive       = 0;
        game->interactive_trigger_id = -1;

        if (loc) {
            Rect pr = {
                p->x - half_w,
                p->y - (float)PLAYER_SPRITE_H,
                (float)PLAYER_W, (float)PLAYER_SPRITE_H
            };
            for (int i = 0; i < loc->trigger_count; i++) {
                TriggerZone *tz = &loc->triggers[i];

                if (tz->target_location_id >= 0) {
                    /* Room transition */
                    if (rect_overlaps(&pr, &tz->bounds)) {
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
                        strncpy(game->interact_label, "Press E to talk",
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

        /* ── Story chapter auto-advance ── */
        if (game->story) {
            int ch = game->story->current_chapter;
            if (ch == 0 && (p->flags & FLAG_MET_LILY))
                story_advance_chapter(game->story, p, game->world);
            else if (ch == 1 && (p->flags & FLAG_FOUND_DIARY))
                story_advance_chapter(game->story, p, game->world);
            else if (ch == 2 && (p->flags & FLAG_OPENED_BASEMENT))
                story_advance_chapter(game->story, p, game->world);
            else if (ch == 3 && (p->flags & FLAG_KNOWS_TRUTH))
                story_advance_chapter(game->story, p, game->world);
        }

        /* ── Danger zone: sanity drain ── */
        if (loc && loc->is_danger_zone) {
            static float danger_timer = 0.0f;
            danger_timer += dt;
            if (danger_timer >= 5.0f) {
                danger_timer = 0.0f;
                player_modify_sanity(p, -2);
            }
        }

    } else if (game->state == GAME_STATE_DIALOGUE && game->player) {
        dialogue_state_update(&game->dialogue_state, dt);
    } else if (game->state == GAME_STATE_ENDING) {
        game->ending_timer += dt;
    }
}

/* ── Rendering ──────────────────────────────────────────────────────────── */

void game_render(Game *game)
{
    if (!game) return;
    switch (game->state) {
    case GAME_STATE_MENU:      game_render_menu(game);      break;
    case GAME_STATE_PLAYING:   game_render_playing(game);   break;
    case GAME_STATE_DIALOGUE:
        game_render_playing(game);
        game_render_dialogue_overlay(game);
        break;
    case GAME_STATE_INVENTORY: game_render_inventory(game); break;
    case GAME_STATE_PAUSE:
        game_render_playing(game);
        game_render_pause(game);
        break;
    case GAME_STATE_ENDING:    game_render_ending(game);    break;
    default: break;
    }
}

/* ── Menu ────────────────────────────────────────────────────────────────── */

void game_render_menu(Game *game)
{
    SDL_Renderer *r = game->renderer;

    /* Gradient background */
    for (int y = 0; y < WINDOW_H; y++) {
        Uint8 c = (Uint8)(y * 25 / WINDOW_H);
        render_filled_rect(r, 0, y, WINDOW_W, 1,
                           (Uint8)(5 + c), (Uint8)(3 + c/2),
                           (Uint8)(8 + c*2), 255);
    }

    render_text_centered(r, "PROJECT YOZORA",
                         WINDOW_W/2, 110, 4, 190, 150, 220);
    render_text_centered(r, "A Horror Story",
                         WINDOW_W/2, 165, 2, 110, 85, 135);
    render_filled_rect(r, WINDOW_W/2 - 150, 196, 300, 2, 70,45,90,180);

    for (int i = 0; i < 3; i++) {
        Button btn = game->buttons[i];
        if (i == game->current_menu_choice) btn.is_hovered = 1;
        draw_button(r, &btn);
    }

    render_text_centered(r,
        "WASD: move | E: interact | I: inventory | ESC: pause",
        WINDOW_W/2, WINDOW_H - 28, 1, 65, 50, 78);
}

/* ── Playing ─────────────────────────────────────────────────────────────── */

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
        render_text(game->renderer, "!",
                    npc_sx - 4,  /* half of "!" glyph width at scale 2 */
                    npc_sy,
                    2, 255, 230, 0);
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

    /* Chapter label */
    if (game->story) {
        static const char *ch_names[] = {
            "Prologue","Chapter I","Chapter II","Chapter III","Finale"
        };
        int ch = game->story->current_chapter;
        if (ch >= 0 && ch < 5)
            render_text(game->renderer, ch_names[ch],
                        WINDOW_W - 100, 12, 1, 90, 72, 110);
    }

    render_text(game->renderer, "I:inv  ESC:pause",
                WINDOW_W - 136, WINDOW_H - 18, 1, 55, 44, 66);
}

/* ── Dialogue overlay ────────────────────────────────────────────────────── */

void game_render_dialogue_overlay(Game *game)
{
    if (!game) return;
    dialogue_render(&game->dialogue_state, game->renderer,
                    WINDOW_W, WINDOW_H);
}

/* ── Inventory ───────────────────────────────────────────────────────────── */

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

    int px = 180, py = 70, pw = WINDOW_W - 360, ph = WINDOW_H - 140;

    render_filled_rect(r, px, py, pw, ph, 15, 10, 25, 235);
    render_rect_outline(r, px, py, pw, ph, 80, 60, 100, 255);
    render_rect_outline(r, px+2, py+2, pw-4, ph-4, 45, 33, 62, 180);

    render_text_centered(r, "INVENTORY", WINDOW_W/2, py + 18, 2, 175, 145, 200);
    render_filled_rect(r, px+18, py+46, pw-36, 2, 55, 42, 70, 190);

    Player *p = game->player;
    if (p->inventory_count == 0) {
        render_text_centered(r, "(empty)", WINDOW_W/2, py + 80, 2, 75, 60, 88);
    } else {
        int iy = py + 60;
        for (int i = 0; i < p->inventory_count; i++) {
            int sy2   = iy + i * 62;
            int is_sel = (i == game->selected_inventory_slot);

            render_filled_rect(r, px+18, sy2, pw-36, 54,
                               is_sel ? 38 : 18,
                               is_sel ? 22 : 12,
                               is_sel ? 55 : 32, 210);
            render_rect_outline(r, px+18, sy2, pw-36, 54,
                                is_sel ? 110 : 45,
                                is_sel ? 72  : 30,
                                is_sel ? 140 : 65, 200);

            render_filled_rect(r, px+28, sy2+9, 36, 36, 90,70,120,220);
            render_rect_outline(r, px+28, sy2+9, 36, 36, 130,100,155,200);

            render_text(r, p->inventory[i].name,
                        px+72, sy2+10, 2,
                        is_sel?225:170, is_sel?195:140, is_sel?250:195);

            render_text_wrapped(r, p->inventory[i].description,
                                px+72, sy2+32, pw-110, 1, 14,
                                is_sel?175:115, is_sel?145:95, is_sel?195:135);
        }
    }

    /* Stat bars in bottom strip */
    int bar_y = py + ph - 36;
    ui_draw_stat_bar(r, px+18, bar_y, 100, 12,
                     p->health,  100, 180,40,40,  "HP");
    ui_draw_stat_bar(r, px+150, bar_y, 100, 12,
                     p->sanity,  100, 40,80,180,  "SN");
    ui_draw_stat_bar(r, px+282, bar_y, 100, 12,
                     p->courage, 100, 180,150,30, "CR");

    render_text_centered(r, "[I] or [ESC] to close",
                         WINDOW_W/2, py+ph-16, 1, 65,52,78);
}

/* ── Pause ───────────────────────────────────────────────────────────────── */

void game_render_pause(Game *game)
{
    if (!game) return;
    SDL_Renderer *r = game->renderer;

    render_filled_rect(r, 0, 0, WINDOW_W, WINDOW_H, 0, 0, 0, 145);

    int pw = 320, ph = 200;
    int qx = (WINDOW_W - pw) / 2, qy = (WINDOW_H - ph) / 2;

    render_filled_rect(r, qx, qy, pw, ph, 15, 10, 25, 235);
    render_rect_outline(r, qx, qy, pw, ph, 80, 60, 100, 255);
    render_text_centered(r, "PAUSED", WINDOW_W/2, qy+18, 2, 175,145,200);
    render_filled_rect(r, qx+18, qy+44, pw-36, 2, 55,42,70, 190);

    for (int i = 0; i < 2; i++) draw_button(r, &game->pause_buttons[i]);
    render_text_centered(r, "[ESC] to resume",
                         WINDOW_W/2, qy+ph-20, 1, 65,52,78);
}

/* ── Ending ──────────────────────────────────────────────────────────────── */

void game_render_ending(Game *game)
{
    if (!game) return;
    SDL_Renderer *r = game->renderer;

    static const char *titles[] = {
        "", "ENDING: ESCAPE",
        "ENDING: SACRIFICE", "ENDING: TRUTH", "ENDING: CORRUPTION"
    };
    static const char *texts[] = {
        "",
        "You sprint through the front door as dawn breaks. Lily's hand is warm "
        "in yours. The estate collapses behind you. Some horrors are best "
        "left in the dark.",
        "You face the creature alone while Lily slips out through the cellar. "
        "The last thing you feel is a strange peace. You chose this.",
        "You speak the creature's true name aloud. The house shudders. "
        "Light floods every shadow. The thing unravels, screaming. "
        "Lily weeps – and smiles.",
        "The darkness was patient. By the time you realise what you have become,"
        " Lily is already gone – and so are you."
    };
    static const Uint8 clrs[][3] = {
        {60,60,60},{30,60,30},{70,30,30},{30,50,70},{50,10,55}
    };

    int et = game->ending_type;
    if (et < 0 || et > 4) et = 0;

    float fade = game->ending_timer / 2.5f;
    if (fade > 1.0f) fade = 1.0f;

    render_filled_rect(r, 0, 0, WINDOW_W, WINDOW_H,
                       (Uint8)((float)clrs[et][0] * fade),
                       (Uint8)((float)clrs[et][1] * fade),
                       (Uint8)((float)clrs[et][2] * fade), 255);

    render_text_centered(r, titles[et], WINDOW_W/2, WINDOW_H/2 - 90, 3,
                         (Uint8)(clrs[et][0] + 120),
                         (Uint8)(clrs[et][1] + 120),
                         (Uint8)(clrs[et][2] + 120));

    render_filled_rect(r, WINDOW_W/2-180, WINDOW_H/2-32, 360, 2,
                       100,80,120, 200);

    if (game->ending_timer > 1.2f) {
        render_text_wrapped(r, texts[et], 180, WINDOW_H/2-10,
                            WINDOW_W-360, 1, 22, 175,155,195);
    }

    if (game->ending_timer > 3.0f) {
        Uint64 t = SDL_GetTicks();
        if ((t / 600) % 2 == 0)
            render_text_centered(r, "[ Press any key to continue ]",
                                 WINDOW_W/2, WINDOW_H-56, 1, 90,72,110);
    }
}
