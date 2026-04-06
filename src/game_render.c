#include "game.h"
#include "effects.h"
#include "player.h"
#include "world.h"
#include "render.h"
#include "camera.h"
#include "ui.h"
#include "story.h"
#include "dialogue.h"
#include "video.h"

#include <string.h>
#include <stdio.h>

/* ── Lab overlay alpha ──────────────────────────────────────────────────── */
/* Alpha of the green gas overlay (0-255). */
#define LAB_GAS_OVERLAY_ALPHA 70

/* ── Inventory grid layout ──────────────────────────────────────────────── */
#define INV_COLS       4
#define INV_CELL_SIZE  84   /* square cell size in px          */
#define INV_CELL_GAP   10   /* gap between cells               */
#define INV_ICON_PAD    8   /* padding inside cell to icon box */

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static SDL_Texture *get_item_icon_texture(const Game *game, int item_id)
{
    if (!game) return NULL;
    switch (item_id) {
    case ITEM_ID_FLASHLIGHT: return game->item_flashlight_texture;
    case ITEM_ID_GASMASK:    return game->item_gasmask_texture;
    case ITEM_ID_KEYCARD:    return game->item_keycard_texture;
    default:                 return NULL;
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

    /* Overlay: gas mask vignette takes precedence when active; otherwise
     * the archive room applies its own darkness (location 0 only). */
    if (game->gasmask_active)
        render_gasmask_vignette(game);
    else
        render_archive_darkness(game);

    /* Lab poisonous gas: green tint overlay when in the lab */
    if (game->player->current_location_id == LOCATION_LAB) {
        render_filled_rect(game->renderer, 0, 0, WINDOW_W, WINDOW_H,
                           0, 200, 30, LAB_GAS_OVERLAY_ALPHA);
    }

    /* Ambient darkness: subtle black overlay to give all rooms a dimmer,
     * more atmospheric look without completely obscuring details. */
    render_filled_rect(game->renderer, 0, 0, WINDOW_W, WINDOW_H, 0, 0, 0, 55);

    /* Flashlight beam (additive warm glow, rendered on top of the darkness) */
    render_flashlight_beam(game);

    /* Stranger NPC: draw a bright yellow exclamation mark above its head
     * when in the Entrance Hall (location 0) so the player can spot it.
     * The body/head are already drawn by world_render_room() via decor,
     * so we only add the visual indicator here. */

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
        if (!game->passcode_active && !game->show_containment_level) {
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

            /* AM recording interactable outline and description */
            int am_hovering = (mx >= AM_RECORD_X &&
                               mx <= AM_RECORD_X + AM_RECORD_W &&
                               my >= AM_RECORD_Y &&
                               my <= AM_RECORD_Y + AM_RECORD_H);
            render_rect_outline(r,
                AM_RECORD_X, AM_RECORD_Y,
                AM_RECORD_W, AM_RECORD_H,
                0, am_hovering ? 255 : 160, am_hovering ? 80 : 0,
                am_hovering ? 255 : 140);
            if (am_hovering) {
                render_text_centered(r, "experiment recording #1",
                    AM_RECORD_X + AM_RECORD_W / 2,
                    AM_RECORD_Y - 16, 1, 0, 220, 255);
            }

            /* Containment level interactable outline and description */
            int cl_hovering = (mx >= CONTAINMENT_LEVEL_RECT_X &&
                               mx <= CONTAINMENT_LEVEL_RECT_X + CONTAINMENT_LEVEL_RECT_W &&
                               my >= CONTAINMENT_LEVEL_RECT_Y &&
                               my <= CONTAINMENT_LEVEL_RECT_Y + CONTAINMENT_LEVEL_RECT_H);
            render_rect_outline(r,
                CONTAINMENT_LEVEL_RECT_X, CONTAINMENT_LEVEL_RECT_Y,
                CONTAINMENT_LEVEL_RECT_W, CONTAINMENT_LEVEL_RECT_H,
                0, cl_hovering ? 255 : 160, cl_hovering ? 80 : 0,
                cl_hovering ? 255 : 140);
            if (cl_hovering) {
                render_text_centered(r, "Containment level",
                    CONTAINMENT_LEVEL_RECT_X + CONTAINMENT_LEVEL_RECT_W / 2,
                    CONTAINMENT_LEVEL_RECT_Y - 16, 1, 0, 220, 255);
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
        /* Containment level overlay (shown when containment rect is clicked) */
        if (game->show_containment_level && game->containment_level_texture) {
            render_texture(r, game->containment_level_texture,
                           0, 0, WINDOW_W, WINDOW_H);
            render_text_centered(r, "Press E or ESC to close",
                                 WINDOW_W / 2, WINDOW_H - 28, 1, 200, 200, 200);
        }
    } else {
        render_text_centered(r, "Press E or ESC to exit",
                             WINDOW_W / 2, WINDOW_H - 28, 1, 200, 200, 200);
    }
} 

/* ── Simon Says minigame ─────────────────────────────────────────────────── */

void game_render_simon(Game *game)
{
    if (!game) return;
    SDL_Renderer *r = game->renderer;

    /* Dark background */
    render_filled_rect(r, 0, 0, WINDOW_W, WINDOW_H, 10, 10, 20, 255);

    /* Panel */
    int px = 390, py = 130, pw = 500, ph = 460;
    render_filled_rect(r, px, py, pw, ph, 30, 30, 50, 255);
    render_rect_outline(r, px, py, pw, ph, 80, 80, 120, 255);

    /* Title and instruction */
    render_text_centered(r, "GENERATOR CONTROL PANEL",
                         WINDOW_W / 2, py + 14, 2, 200, 200, 255);

    char buf[64];
    if (game->simon_phase == 0 || game->simon_phase == 2) {
        snprintf(buf, sizeof(buf), "Watch the sequence  (round %d / 10)",
                 game->simon_length);
        render_text_centered(r, buf, WINDOW_W / 2, py + 38, 1, 160, 160, 200);
    } else {
        snprintf(buf, sizeof(buf), "Repeat the sequence  (%d / %d)",
                 game->simon_player_pos, game->simon_length);
        render_text_centered(r, buf, WINDOW_W / 2, py + 38, 1, 200, 220, 160);
    }

    /* Button definitions: x, y, w, h, dim colour, lit colour, label */
    static const int bx[4] = { 450, 670, 450, 670 };
    static const int by[4] = { 190, 190, 370, 370 };
    static const int bw = 160, bh = 160;

    /* dim colours */
    static const Uint8 dim_r[4] = { 120,  20,  20, 120 };
    static const Uint8 dim_g[4] = {  20,  20, 120, 120 };
    static const Uint8 dim_b[4] = {  20, 120,  20,  20 };
    /* lit colours */
    static const Uint8 lit_r[4] = { 255,  50,  50, 255 };
    static const Uint8 lit_g[4] = {  50,  50, 255, 255 };
    static const Uint8 lit_b[4] = {  50, 255,  50,  50 };

    static const char *labels[4] = { "RED", "BLUE", "GREEN", "YELLOW" };

    for (int i = 0; i < 4; i++) {
        int lit = (game->simon_lit_button == i);
        Uint8 cr = lit ? lit_r[i] : dim_r[i];
        Uint8 cg = lit ? lit_g[i] : dim_g[i];
        Uint8 cb = lit ? lit_b[i] : dim_b[i];
        render_filled_rect(r, bx[i], by[i], bw, bh, cr, cg, cb, 255);
        render_rect_outline(r, bx[i], by[i], bw, bh, 200, 200, 200, 200);
        render_text_centered(r, labels[i],
                             bx[i] + bw / 2, by[i] + bh / 2 - 8,
                             2, 255, 255, 255);
    }

    render_text_centered(r, "Click the correct buttons in order",
                         WINDOW_W / 2, py + ph - 24, 1, 120, 120, 160);
}

/* ── Jumpscare video ─────────────────────────────────────────────────────── */

void game_render_jumpscare(Game *game)
{
    if (!game) return;
    SDL_Renderer *r = game->renderer;

    /* Black background in case the video hasn't decoded its first frame yet */
    render_filled_rect(r, 0, 0, WINDOW_W, WINDOW_H, 0, 0, 0, 255);

    video_player_render(game->jumpscare_player, r);
}
