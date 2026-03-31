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
        render_filled_rect(renderer, x, y, w, h, 90, 20, 20, 240);
    else
        render_filled_rect(renderer, x, y, w, h, 45, 10, 10, 230);

    /* Border */
    render_rect_outline(renderer, x,   y,   w,   h,   140,30,30,255);
    render_rect_outline(renderer, x+2, y+2, w-4, h-4,  70,15,15, 180);

    /* Text */
    if (button->text) {
        int tw = render_text_width(button->text, 2);
        int tx = x + (w - tw) / 2;
        int ty = y + (h - 16) / 2;
        if (button->is_hovered)
            render_text(renderer, button->text, tx, ty, 2, 255, 220, 220);
        else
            render_text(renderer, button->text, tx, ty, 2, 200, 130, 130);
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

    /* Text (red on hover, light red when not) */
    if (button->text) {
        int tw = render_text_width(button->text, 2);
        int tx = x + (w - tw) / 2;
        int ty = y + (h - 16) / 2;

        if (button->is_hovered)
            render_text(renderer, button->text, tx, ty, 2, 255, 0, 0);      /* bright red text on hover */
        else
            render_text(renderer, button->text, tx, ty, 2, 210, 160, 160); /* light red */
    }
}
/* ── Slider ────────────────────────────────────────────────────────────── */

void slider_init(Slider *s, float x, float y, float w, float h,
                 float min_val, float max_val, float value)
{
    if (!s) return;
    s->x = x; s->y = y;
    s->w = w; s->h = h;
    s->min = min_val; s->max = max_val;
    s->focused = 0;
    slider_set_value(s, value);
}

void slider_set_value(Slider *s, float value)
{
    if (!s) return;
    if (value < s->min) value = s->min;
    if (value > s->max) value = s->max;
    s->value = value;
}

int slider_handle_click(Slider *s, float mx, float my)
{
    if (!s) return 0;
    if (mx >= s->x && mx <= s->x + s->w &&
        my >= s->y - 10.0f && my <= s->y + s->h + 10.0f) {
        float t = (mx - s->x) / s->w;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        s->value = s->min + t * (s->max - s->min);
        return 1;
    }
    return 0;
}

void slider_render(SDL_Renderer *r, const Slider *s, const char *label)
{
    if (!r || !s) return;

    int x = (int)s->x, y = (int)s->y;
    int w = (int)s->w, h = (int)s->h;

    /* Label (right-aligned, left of the track) */
    if (label) {
        int lw = render_text_width(label, 2);
        int lx = x - lw - 20;
        int ly = y + (h - 16) / 2;
        if (s->focused)
            render_text(r, label, lx, ly, 2, 255, 190, 190);
        else
            render_text(r, label, lx, ly, 2, 180, 110, 110);
    }

    /* Track background */
    render_filled_rect(r, x, y, w, h, 40, 10, 10, 200);
    render_rect_outline(r, x, y, w, h,
        s->focused ? 170 : 105,
        s->focused ?  40 :  25,
        s->focused ?  40 :  25,
        200);

    /* Filled portion */
    float t = (s->max > s->min) ? (s->value - s->min) / (s->max - s->min) : 0.0f;
    int filled_w = (int)(w * t);
    if (filled_w > 0) {
        render_filled_rect(r, x, y, filled_w, h,
            s->focused ? 155 : 110,
            s->focused ?  35 :  25,
            s->focused ?  35 :  25,
            220);
    }

    /* Handle */
    int handle_x = x + filled_w - 6;
    if (handle_x < x) handle_x = x;
    if (handle_x > x + w - 12) handle_x = x + w - 12;
    render_filled_rect(r, handle_x, y - 4, 12, h + 8,
        s->focused ? 255 : 185,
        s->focused ? 150 :  90,
        s->focused ? 150 :  90,
        255);

    /* Value text (right of the track) */
    char val_buf[16];
    snprintf(val_buf, sizeof(val_buf), "%d", (int)s->value);
    render_text(r, val_buf, x + w + 16, y + (h - 16) / 2, 2,
        s->focused ? 255 : 195,
        s->focused ? 190 : 115,
        s->focused ? 190 : 115);
}

/* ── HUD ───────────────────────────────────────────────────────────────── */

void ui_draw_hud(SDL_Renderer *renderer, const Player *player)
{
    if (!renderer || !player) return;

    /* Inventory count */
    char inv_text[32];
    snprintf(inv_text, sizeof(inv_text), "Items: %d/%d",
             player->inventory_count, INVENTORY_CAPACITY);
    render_text(renderer, inv_text, 14, 14, 1, 150, 90, 90);
}

/* ── Objective bar ─────────────────────────────────────────────────────── */

void ui_draw_objective_bar(SDL_Renderer *renderer, const char *objective_text)
{
    if (!renderer || !objective_text) return;

    /* Build display string */
    char buf[128];
    snprintf(buf, sizeof(buf), "OBJECTIVE: %s", objective_text);

    int scale  = 2;
    int pad_x  = 16;
    int pad_y  = 8;
    int text_w = render_text_width(buf, scale);
    int text_h = 8 * scale;

    /* Centre the box horizontally, pin to top of screen */
    int bar_w  = text_w + pad_x * 2;
    int bar_h  = text_h + pad_y * 2;
    int bar_x  = (WINDOW_W - bar_w) / 2;
    int bar_y  = 0;

    /* Background */
    render_filled_rect(renderer, bar_x, bar_y, bar_w, bar_h,
                       10, 3, 3, 200);
    /* Border */
    render_rect_outline(renderer, bar_x, bar_y, bar_w, bar_h,
                        130, 30, 30, 220);

    /* Text */
    render_text(renderer, buf,
                bar_x + pad_x, bar_y + pad_y,
                scale, 210, 150, 150);
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
    render_filled_rect(renderer, tx - 6, ty - 4, tw + 12, 20, 30,8,8, 200);
    render_rect_outline(renderer, tx - 6, ty - 4, tw + 12, 20, 110,25,25, 220);

    render_text(renderer, buf, tx, ty, 1, 255, 195, 195);
}
