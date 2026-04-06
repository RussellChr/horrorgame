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
#include "effects.h"
#include "interactions.h"
#include "video.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -- Lab poisonous-gas constants ------------------------------------------ */
/* Seconds the player can stay in the lab without a gas mask before dying. */
#define LAB_GAS_DEATH_DELAY  3.0f

/* ── Simon Says constants ──────────────────────────────────────────────── */
#define SIMON_LIT_DURATION   0.55f  /* seconds a button stays lit        */
#define SIMON_GAP_DURATION   0.20f  /* dark gap between lit steps         */
#define SIMON_ROUND_PAUSE    0.60f  /* pause between a correct round and the next show */
#define SIMON_START_PAUSE    0.40f  /* brief pause before first show      */
#define SIMON_JUMPSCARE_ROUND   7   /* round at which the jumpscare fires */
#define SIMON_JUMPSCARE_DELAY 1.0f  /* seconds to wait before showing it */

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

    /* Seed the PRNG once at startup (used by the Simon minigame) */
    srand((unsigned int)SDL_GetTicks());

    /* Rare ambient flicker defaults */
    g->ambient_flicker_timer    = 7.0f + (float)(rand() % 12); /* 7-18s */
    g->ambient_flicker_duration = 0.0f;
    g->ambient_flicker_alpha    = 0;

    /* Load the dialogue box background image */
    dialogue_load_texture(&g->dialogue_state, renderer, "assets/dialogue.png");

    /* Load inner monologue file */
    monologue_load(&g->monologue_file, "assets/dialogue/monologues.txt");

    /* Load Title Screen*/
    g->title_screen_texture = render_load_texture(renderer, "assets/title_screen.png");

    /* Load locker view */
    g->locker_texture      = render_load_texture(renderer, "assets/locker.png");
    g->note_locker_texture = render_load_texture(renderer, "assets/note_locker.png");
    /* Load monitor zoom texture */
    g->monitor_zoom_texture = render_load_texture(renderer, "assets/monitor_zoom.png");
    /* Load containment level overlay texture */
    g->containment_level_texture = render_load_texture(renderer, "assets/containment_level.png");

    /* Load AM recording audio */
    if (SDL_LoadWAV("assets/AM.wav", &g->am_wav_spec,
                    &g->am_wav_buf, &g->am_wav_len)) {
        g->am_audio_stream = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &g->am_wav_spec, NULL, NULL);
        if (!g->am_audio_stream)
            SDL_Log("game_init: failed to open audio stream: %s", SDL_GetError());
    } else {
        SDL_Log("game_init: failed to load AM.wav: %s", SDL_GetError());
    }

    /* Load inventory item icons */
    g->item_flashlight_texture = render_load_texture(renderer, "assets/flashlight.png");
    g->item_gasmask_texture    = render_load_texture(renderer, "assets/gasmask.png");
    g->item_keycard_texture    = render_load_texture(renderer, "assets/keycard.png");

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
    render_texture_destroy(game->containment_level_texture);
    if (game->am_audio_stream) SDL_DestroyAudioStream(game->am_audio_stream);
    if (game->am_wav_buf)      SDL_free(game->am_wav_buf);
    video_player_close(game->jumpscare_player);
    game->jumpscare_player = NULL;
    render_texture_destroy(game->item_flashlight_texture);
    render_texture_destroy(game->item_gasmask_texture);
    render_texture_destroy(game->item_keycard_texture);
    render_texture_destroy(game->dark_overlay);
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
    game->lab_gas_timer           = LAB_GAS_DEATH_DELAY;
    game->lab_death_triggered     = 0;
    game->ambient_flicker_timer    = 5.0f + (float)(rand() % 7); /* 5-11s */
    game->ambient_flicker_duration = 0.0f;
    game->ambient_flicker_alpha    = 0;

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

    /* Reset lab gas death state whenever the player changes rooms */
    game->lab_gas_timer      = LAB_GAS_DEATH_DELAY;
    game->lab_death_triggered = 0;

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
    } else if (game->lab_death_triggered) {
        /* Player died from the lab gas — return to main menu */
        game->lab_death_triggered = 0;
        game->lab_gas_timer       = LAB_GAS_DEATH_DELAY;
        game->state               = GAME_STATE_MENU;
    } else if (game->simon_death_triggered) {
        /* Player failed the Simon game — return to main menu */
        game->simon_death_triggered = 0;
        game->state                 = GAME_STATE_MENU;
    } else {
        game->state = GAME_STATE_PLAYING;
    }
}

/* ── Simon Says minigame ────────────────────────────────────────────────── */

void game_start_simon(Game *game)
{
    if (!game) return;
    for (int i = 0; i < 10; i++)
        game->simon_sequence[i] = rand() % 4;
    game->simon_length      = 1;
    game->simon_show_pos    = 0;
    game->simon_show_lit    = 0;
    game->simon_show_timer  = SIMON_START_PAUSE;
    game->simon_player_pos  = 0;
    game->simon_phase       = 0; /* showing phase */
    game->simon_lit_button  = -1;
    game->simon_death_triggered = 0;
    game->state = GAME_STATE_SIMON;
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
                    game_handle_interaction(game);
                break;
            default: break;
            }
        }
        break;

    case GAME_STATE_LOCKER:
        if (game->show_monitor_zoom && !game->passcode_active &&
            !game->show_containment_level) {
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
                /* Check if the AM recording interactable was clicked */
                if (mx >= AM_RECORD_X && mx <= AM_RECORD_X + AM_RECORD_W &&
                    my >= AM_RECORD_Y && my <= AM_RECORD_Y + AM_RECORD_H) {
                    if (game->am_audio_stream && game->am_wav_buf &&
                        game->am_wav_len <= (Uint32)SDL_MAX_SINT32) {
                        SDL_ClearAudioStream(game->am_audio_stream);
                        SDL_PutAudioStreamData(game->am_audio_stream,
                                               game->am_wav_buf,
                                               (int)game->am_wav_len);
                        SDL_ResumeAudioStreamDevice(game->am_audio_stream);
                    }
                }
                /* Check if the containment level rect was clicked */
                if (mx >= CONTAINMENT_LEVEL_RECT_X &&
                    mx <= CONTAINMENT_LEVEL_RECT_X + CONTAINMENT_LEVEL_RECT_W &&
                    my >= CONTAINMENT_LEVEL_RECT_Y &&
                    my <= CONTAINMENT_LEVEL_RECT_Y + CONTAINMENT_LEVEL_RECT_H) {
                    game->show_containment_level = 1;
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
                /* If containment level overlay is open, just close it */
                if (game->show_containment_level) {
                    game->show_containment_level = 0;
                } else {
                    game->show_note_locker       = 0;
                    game->show_monitor_zoom      = 0;
                    game->show_containment_level = 0;
                    game->passcode_active        = 0;
                    game->passcode_input_len     = 0;
                    game->passcode_input[0]      = '\0';
                    game->passcode_wrong         = 0;
                    game->passcode_correct       = 0;
                    game->state = GAME_STATE_PLAYING;
                }
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
                game_apply_dialogue_choice_flag(game);
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
            game_apply_dialogue_choice_flag(game);
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
                    } else if (it->usable && it->id == ITEM_ID_GASMASK) {
                        game->gasmask_active = !game->gasmask_active;
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

    case GAME_STATE_SIMON:
        /* Only accept mouse clicks during player-input phase */
        if (game->simon_phase == 1 &&
            event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            /* Determine which button (0=Red,1=Blue,2=Green,3=Yellow) was clicked.
               Button layout (screen coords, see game_render_simon):
                 Red   (450, 190, 160, 160)
                 Blue  (670, 190, 160, 160)
                 Green (450, 370, 160, 160)
                 Yellow(670, 370, 160, 160) */
            int clicked = -1;
            float mx = game->mouse_x, my = game->mouse_y;
            if (mx >= 450 && mx < 610) {
                if      (my >= 190 && my < 350) clicked = 0; /* Red   */
                else if (my >= 370 && my < 530) clicked = 2; /* Green */
            } else if (mx >= 670 && mx < 830) {
                if      (my >= 190 && my < 350) clicked = 1; /* Blue   */
                else if (my >= 370 && my < 530) clicked = 3; /* Yellow */
            }
            if (clicked >= 0) {
                game->simon_lit_button = clicked;
                if (clicked == game->simon_sequence[game->simon_player_pos]) {
                    game->simon_player_pos++;
                    if (game->simon_player_pos >= game->simon_length) {
                        /* Completed this round */
                        if (game->simon_length >= 10) {
                            /* Player won — power the generator */
                            player_set_flag(game->player, FLAG_POWER_GENERATOR_ON);
                            game->simon_lit_button = -1;
                            game_set_dialogue_tree(game, "power_generator_win", 4);
                            if (game->dialogue_tree)
                                game_start_dialogue(game, 0);
                        } else {
                            /* Start next round after a brief pause */
                            game->simon_length++;
                            game->simon_player_pos = 0;
                            game->simon_phase      = 2; /* round-pause */
                            game->simon_show_timer = SIMON_ROUND_PAUSE;
                            game->simon_lit_button = -1;
                        }
                    }
                } else {
                    /* Wrong button — player dies */
                    game->simon_death_triggered = 1;
                    game->simon_lit_button = -1;
                    game_set_dialogue_tree(game, "power_simon_fail", 4);
                    if (game->dialogue_tree)
                        game_start_dialogue(game, 0);
                }
            }
        }
        break;

    case GAME_STATE_JUMPSCARE:
        /* Allow the player to skip the jumpscare by pressing Escape */
        if (event->type == SDL_EVENT_KEY_DOWN &&
            event->key.scancode == SDL_SCANCODE_ESCAPE) {
            video_player_close(game->jumpscare_player);
            game->jumpscare_player = NULL;
            game->simon_phase      = 1; /* resume player-input phase */
            game->simon_player_pos = 0;
            game->state            = GAME_STATE_SIMON;
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
                        else if (tz->trigger_id == 61)
                            label = "Press [E] to examine";
                        else if (tz->trigger_id == 75)
                            label = "Press [E] to interact";
                        else if (tz->trigger_id == 91)
                            label = "Press [E] to examine";
                        else if (tz->trigger_id == 92)
                            label = "Press [E] to examine";
                        else if (tz->trigger_id == 95)
                            label = "Press [E] to interact";
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

        /* ── Rare subtle ambient flicker ── */
        if (game->ambient_flicker_duration > 0.0f) {
            game->ambient_flicker_duration -= dt;
            if (game->ambient_flicker_duration < 0.0f)
                game->ambient_flicker_duration = 0.0f;
        } else {
            game->ambient_flicker_alpha = 0;
            game->ambient_flicker_timer -= dt;
            if (game->ambient_flicker_timer <= 0.0f) {
                /* Single short pulse, low intensity, and infrequent */
                game->ambient_flicker_duration = 0.07f + (float)(rand() % 7) / 100.0f; /* 0.07-0.13s */
                game->ambient_flicker_alpha    = (Uint8)(3 + (rand() % 6));             /* additive 3-8 alpha */
                game->ambient_flicker_timer    = 7.0f + (float)(rand() % 12);            /* next pulse in 7-18s */
            }
        }

        /* ── Lab poisonous gas: kill player if in lab without gas mask ── */
        if (p->current_location_id == LOCATION_LAB &&
            !game->lab_death_triggered) {
            if (!game->gasmask_active) {
                game->lab_gas_timer -= dt;
                if (game->lab_gas_timer <= 0.0f) {
                    game->lab_death_triggered = 1;
                    game_set_dialogue_tree(game, "lab_gas_death", LOCATION_LAB);
                    if (game->dialogue_tree)
                        game_start_dialogue(game, 0);
                }
            } else {
                /* Gas mask is on — replenish the timer */
                game->lab_gas_timer = LAB_GAS_DEATH_DELAY;
            }
        } else if (p->current_location_id != LOCATION_LAB) {
            /* Outside the lab — reset the timer for the next visit */
            game->lab_gas_timer = LAB_GAS_DEATH_DELAY;
        }

    } else if (game->state == GAME_STATE_DIALOGUE && game->player) {
        dialogue_state_update(&game->dialogue_state, dt);
    } else if (game->state == GAME_STATE_SIMON) {
        /* ── Simon Says update ── */
        game->simon_show_timer -= dt;
        if (game->simon_phase == 0) {
            /* Showing the sequence */
            if (game->simon_show_timer <= 0.0f) {
                if (!game->simon_show_lit) {
                    /* Gap/initial-pause ended – light up current button */
                    game->simon_show_lit   = 1;
                    game->simon_lit_button = game->simon_sequence[game->simon_show_pos];
                    game->simon_show_timer = SIMON_LIT_DURATION;
                } else {
                    /* Lit period ended – unlight */
                    game->simon_show_lit   = 0;
                    game->simon_lit_button = -1;
                    if (game->simon_show_pos + 1 >= game->simon_length) {
                        /* All steps shown for this round */
                        if (game->simon_length == SIMON_JUMPSCARE_ROUND) {
                            /* Round 7: wait 1 s then show the jumpscare */
                            game->simon_phase      = 3;
                            game->simon_show_timer = SIMON_JUMPSCARE_DELAY;
                        } else {
                            /* All other rounds: go straight to player input */
                            game->simon_phase      = 1;
                            game->simon_player_pos = 0;
                        }
                    } else {
                        /* Advance to next button after a gap */
                        game->simon_show_pos++;
                        game->simon_show_timer = SIMON_GAP_DURATION;
                    }
                }
            }
        } else if (game->simon_phase == 2) {
            /* Brief pause between rounds */
            if (game->simon_show_timer <= 0.0f) {
                /* Begin showing the new (longer) sequence */
                game->simon_phase      = 0;
                game->simon_show_pos   = 0;
                game->simon_show_lit   = 0;
                game->simon_lit_button = -1;
                game->simon_show_timer = SIMON_START_PAUSE;
            }
        } else if (game->simon_phase == 3) {
            /* Pre-jumpscare delay after round 7 pattern display */
            if (game->simon_show_timer <= 0.0f) {
                game->jumpscare_player =
                    video_player_open(game->renderer, "assets/jumpscare.mov");
                if (game->jumpscare_player) {
                    game->state = GAME_STATE_JUMPSCARE;
                } else {
                    /* File missing or unreadable — skip straight to player input */
                    SDL_Log("game: jumpscare.mov could not be opened; skipping");
                    game->simon_phase      = 1;
                    game->simon_player_pos = 0;
                }
            }
        }
    } else if (game->state == GAME_STATE_JUMPSCARE) {
        /* ── Jumpscare video update ── */
        video_player_update(game->jumpscare_player, dt);
        if (video_player_is_done(game->jumpscare_player)) {
            video_player_close(game->jumpscare_player);
            game->jumpscare_player = NULL;
            game->simon_phase      = 1; /* resume player-input phase */
            game->simon_player_pos = 0;
            game->state            = GAME_STATE_SIMON;
        }
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
    case GAME_STATE_SIMON:     game_render_simon(game);     break;
    case GAME_STATE_JUMPSCARE: game_render_jumpscare(game); break;
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
    if (game->brightness < 100.0f && game->state != GAME_STATE_SETTINGS
        && game->state != GAME_STATE_JUMPSCARE) {
        Uint8 alpha = (Uint8)((1.0f - game->brightness / 100.0f) * 220.0f);
        render_filled_rect(game->renderer, 0, 0, WINDOW_W, WINDOW_H,
                           0, 0, 0, alpha);
    }
}
