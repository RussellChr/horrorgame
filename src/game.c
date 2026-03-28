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

    /* Load inventory UI textures */
    g->inventory_bg_texture   = render_load_texture(renderer,
                                                    "assets/inventory_bg.png");
    g->inventory_slot_texture = render_load_texture(renderer,
                                                    "assets/inventory_slot.png");
    /* Load Title Screen*/
    g->title_screen_texture = render_load_texture(renderer, "assets/title_screen.png");

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
    render_texture_destroy(game->inventory_bg_texture);
    render_texture_destroy(game->inventory_slot_texture);
    render_texture_destroy(game->title_screen_texture);
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
        set_dialogue_tree(game, "key", 4);
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
        set_dialogue_tree(game, "diary", 2);
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
            set_dialogue_tree(game, "basement_door_unlocked", 1 + 100);
        } else {
            set_dialogue_tree(game, "basement_door_locked", 1);
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
        set_dialogue_tree(game, "ritual_circle", 5);
        /* Check for ending condition after ritual */
        if (game->player->flags & FLAG_KNOWS_TRUTH) {
            /* End will be triggered after dialogue */
            game->ending_type = -1; /* signal to trigger ending on dialogue end */
        }
    }
    /* Portrait interaction (Entrance Hall, trigger 30) */
    else if (tid == 30 && loc_id == 0) {
        set_dialogue_tree(game, "portrait", 30);
    }
    /* Stranger NPC interaction (Entrance Hall, trigger 40) */
    else if (tid == 40 && loc_id == 0) {
        set_dialogue_tree(game, "stranger", 40);
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
            /* Apply story flag before advancing on mouse click too */
            apply_dialogue_choice_flag(game);
            int cont = dialogue_state_advance(&game->dialogue_state, 0);
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
    case GAME_STATE_SETTINGS:  game_render_settings(game);  break;
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
                        WINDOW_W - 100, 12, 1, 110, 30, 30);
    }

    render_text(game->renderer, "I:inv  ESC:pause",
                WINDOW_W - 136, WINDOW_H - 18, 1, 66, 18, 18);
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

    /* Inventory panel background: PNG if available, otherwise solid rect */
    if (game->inventory_bg_texture) {
        render_texture(r, game->inventory_bg_texture, px, py, pw, ph);
    } else {
        render_filled_rect(r, px, py, pw, ph, 25, 8, 8, 235);
        render_rect_outline(r, px, py, pw, ph, 110, 25, 25, 255);
        render_rect_outline(r, px+2, py+2, pw-4, ph-4, 55, 15, 15, 180);
    }

    render_text_centered(r, "INVENTORY", WINDOW_W/2, py + 18, 2, 200, 110, 110);
    render_filled_rect(r, px+18, py+46, pw-36, 2, 70, 18, 18, 190);

    Player *p = game->player;
    if (p->inventory_count == 0) {
        render_text_centered(r, "(empty)", WINDOW_W/2, py + 80, 2, 88, 28, 28);
    } else {
        int iy = py + 60;
        for (int i = 0; i < p->inventory_count; i++) {
            int sy2   = iy + i * 62;
            int is_sel = (i == game->selected_inventory_slot);

            /* Item slot: PNG if available, otherwise solid rect */
            if (game->inventory_slot_texture) {
                render_texture(r, game->inventory_slot_texture,
                               px+18, sy2, pw-36, 54);
            } else {
                render_filled_rect(r, px+18, sy2, pw-36, 54,
                                   is_sel ? 55 : 32,
                                   is_sel ?  12 :  8,
                                   is_sel ?  12 :  8, 210);
                render_rect_outline(r, px+18, sy2, pw-36, 54,
                                    is_sel ? 140 :  65,
                                    is_sel ?  30 :  15,
                                    is_sel ?  30 :  15, 200);
            }

            /* Selection highlight overlay when slot texture is in use */
            if (game->inventory_slot_texture && is_sel) {
                render_filled_rect(r, px+18, sy2, pw-36, 54,
                                   255, 160, 160, 40);
                render_rect_outline(r, px+18, sy2, pw-36, 54,
                                    220, 100, 100, 160);
            }

            render_filled_rect(r, px+28, sy2+9, 36, 36, 120,25,25,220);
            render_rect_outline(r, px+28, sy2+9, 36, 36, 155,35,35,200);

            render_text(r, p->inventory[i].name,
                        px+72, sy2+10, 2,
                        is_sel?255:195, is_sel?175:115, is_sel?175:115);

            render_text_wrapped(r, p->inventory[i].description,
                                px+72, sy2+32, pw-110, 1, 14,
                                is_sel?195:135, is_sel?115:55, is_sel?115:55);
        }
    }

    render_text_centered(r, "[I] or [ESC] to close",
                         WINDOW_W/2, py+ph-16, 1, 78,22,22);
}

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
                       120,30,30, 200);

    if (game->ending_timer > 1.2f) {
        render_text_wrapped(r, texts[et], 180, WINDOW_H/2-10,
                            WINDOW_W-360, 1, 22, 195,115,115);
    }

    if (game->ending_timer > 3.0f) {
        Uint64 t = SDL_GetTicks();
        if ((t / 600) % 2 == 0)
            render_text_centered(r, "[ Press any key to continue ]",
                                 WINDOW_W/2, WINDOW_H-56, 1, 110,30,30);
    }
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
