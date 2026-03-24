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
void draw_button_menu(SDL_Renderer *renderer, const Button *button)
{
    if (!button || !renderer) return;

    int x = (int)button->rect.x;
    int y = (int)button->rect.y;
    int w = (int)button->rect.w;
    int h = (int)button->rect.h;

    /* Dark overlay when hovered */
    if (button->is_hovered) {
        render_filled_rect(renderer, x, y, w, h, 0, 0, 0, 110);           /* dark transparent */
        
        /* Shadow effect - offset dark rectangle below */
        render_filled_rect(renderer, x + 4, y + h + 2, w, 8, 0, 0, 0, 80);  /* shadow */
    }

    /* Text (black on hover, light purple when not) */
    if (button->text) {
        int tw = render_text_width(button->text, 2);
        int tx = x + (w - tw) / 2;
        int ty = y + (h - 16) / 2;

        if (button->is_hovered)
            render_text(renderer, button->text, tx, ty, 2, 255, 0, 0);      /* black text on hover */
        else
            render_text(renderer, button->text, tx, ty, 2, 200, 190, 210); /* light purple */
    }
}
/* ── HUD ───────────────────────────────────────────────────────────────── */

void ui_draw_hud(SDL_Renderer *renderer, const Player *player)
{
    if (!renderer || !player) return;

    /* Inventory count */
    char inv_text[32];
    snprintf(inv_text, sizeof(inv_text), "Items: %d/%d",
             player->inventory_count, INVENTORY_CAPACITY);
    render_text(renderer, inv_text, 14, 14, 1, 140, 120, 150);
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
