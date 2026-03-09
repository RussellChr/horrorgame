#include "ui.h"
#include "player.h"
#include "render.h"
#include <string.h>
#include <stdio.h>

/* ── Button ───────────────────────────────────────────────────────────── */

void button_update_hover(Button *button, float mouse_x, float mouse_y)
{
    if (!button) return;
    button->is_hovered = button_is_clicked(button, mouse_x, mouse_y);
}

int button_is_clicked(Button *button, float mouse_x, float mouse_y)
{
    if (!button) return 0;
    return (mouse_x >= button->rect.x &&
            mouse_x <= button->rect.x + button->rect.w &&
            mouse_y >= button->rect.y &&
            mouse_y <= button->rect.y + button->rect.h);
}

void draw_button(SDL_Renderer *renderer, const Button *button)
{
    if (!button || !renderer) return;

    int x = (int)button->rect.x;
    int y = (int)button->rect.y;
    int w = (int)button->rect.w;
    int h = (int)button->rect.h;

    /* Shadow */
    render_filled_rect(renderer, x+4, y+4, w, h, 0,0,0,120);

    /* Body */
    if (button->is_hovered)
        render_filled_rect(renderer, x, y, w, h, 80, 50, 90, 240);
    else
        render_filled_rect(renderer, x, y, w, h, 35, 25, 45, 230);

    /* Border */
    render_rect_outline(renderer, x,   y,   w,   h,   120,80,140,255);
    render_rect_outline(renderer, x+2, y+2, w-4, h-4,  60,40,80, 180);

    /* Text */
    if (button->text) {
        int tw = render_text_width(button->text, 2);
        int tx = x + (w - tw) / 2;
        int ty = y + (h - 16) / 2;
        if (button->is_hovered)
            render_text(renderer, button->text, tx, ty, 2, 255, 220, 255);
        else
            render_text(renderer, button->text, tx, ty, 2, 180, 150, 200);
    }
}

/* ── HUD ───────────────────────────────────────────────────────────────── */

void ui_draw_stat_bar(SDL_Renderer *renderer,
                      int x, int y, int w, int h,
                      int value, int max_value,
                      Uint8 r, Uint8 g, Uint8 b,
                      const char *label)
{
    if (!renderer) return;

    /* Background */
    render_filled_rect(renderer, x, y, w, h, 20, 15, 25, 200);
    render_rect_outline(renderer, x, y, w, h, 60, 50, 70, 200);

    /* Fill */
    int fill_w = (max_value > 0) ? (w * value / max_value) : 0;
    if (fill_w > 0)
        render_filled_rect(renderer, x+1, y+1, fill_w-2, h-2, r, g, b, 220);

    /* Label */
    if (label)
        render_text(renderer, label, x + w + 6, y + (h - 8) / 2, 1, 160, 140, 170);
}

void ui_draw_hud(SDL_Renderer *renderer, const Player *player)
{
    if (!renderer || !player) return;

    int bar_x = 14;
    int bar_y = 14;
    int bar_w = 120;
    int bar_h = 12;
    int bar_gap = 18;

    /* Semi-transparent HUD background */
    render_filled_rect(renderer, 8, 8, 200, 72, 10, 8, 15, 160);
    render_rect_outline(renderer, 8, 8, 200, 72, 50, 40, 60, 200);

    /* Health – red */
    ui_draw_stat_bar(renderer, bar_x, bar_y,
                     bar_w, bar_h,
                     player->health, 100,
                     180, 40, 40, "HP");

    /* Sanity – blue */
    ui_draw_stat_bar(renderer, bar_x, bar_y + bar_gap,
                     bar_w, bar_h,
                     player->sanity, 100,
                     40, 80, 180, "SN");

    /* Courage – gold */
    ui_draw_stat_bar(renderer, bar_x, bar_y + bar_gap * 2,
                     bar_w, bar_h,
                     player->courage, 100,
                     180, 150, 30, "CR");

    /* Inventory count */
    char inv_text[32];
    snprintf(inv_text, sizeof(inv_text), "Items: %d/%d",
             player->inventory_count, INVENTORY_CAPACITY);
    render_text(renderer, inv_text, bar_x, bar_y + bar_gap * 3 + 2,
                1, 140, 120, 150);
}

/* ── Interaction prompt ────────────────────────────────────────────────── */

void ui_draw_interact_prompt(SDL_Renderer *renderer,
                             const char *label,
                             int screen_x, int screen_y)
{
    if (!renderer || !label) return;

    char buf[80];
    snprintf(buf, sizeof(buf), "[E] %s", label);
    int tw = render_text_width(buf, 1);
    int tx = screen_x - tw / 2;
    int ty = screen_y - 30;

    /* Background */
    render_filled_rect(renderer, tx - 6, ty - 4, tw + 12, 20, 20,15,30, 200);
    render_rect_outline(renderer, tx - 6, ty - 4, tw + 12, 20, 80,60,100, 220);

    render_text(renderer, buf, tx, ty, 1, 220, 200, 255);
}
