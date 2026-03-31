#include "ui.h"
#include "player.h"
#include "story.h"
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

void ui_draw_objective_bar(SDL_Renderer *renderer, const StoryState *story,
                           const Player *player)
{
    if (!renderer || !story || !player) return;

    static const char *ch_names[] = {
        "Prologue", "Chapter I", "Chapter II", "Chapter III", "Finale"
    };

    int ch = story->current_chapter;
    if (ch < 0 || ch >= CHAPTER_COUNT) return;

    /* Objective text — Chapter II has a two-stage objective driven by flags */
    const char *obj_text;
    switch (ch) {
        case CHAPTER_PROLOGUE: obj_text = "Find Lily";                  break;
        case CHAPTER_ONE:      obj_text = "Find the diary";             break;
        case CHAPTER_TWO:
            obj_text = player_check_flag(player, FLAG_KEY_OBTAINED)
                       ? "Open the basement door"
                       : "Get the basement key";
            break;
        case CHAPTER_THREE:    obj_text = "Discover the truth";         break;
        case CHAPTER_FINALE:   obj_text = "Find a way to escape";       break;
        default:               obj_text = "";                           break;
    }

    const char *ch_name = ch_names[ch];

    /* Bar dimensions */
    int bar_w  = 210;
    int bar_x  = WINDOW_W - bar_w - 14;
    int bar_y  = 10;
    int pad_x  = 8;
    int max_tw = bar_w - pad_x * 2;
    int line_h = 11;

    /* Count wrapped objective lines to size the bar dynamically */
    int obj_lines = 1;
    if (render_text_width(obj_text, 1) > max_tw) obj_lines = 2;
    int bar_h = 34 + obj_lines * line_h;   /* 20 header + 14 gap/pad + lines */

    /* Shadow */
    render_filled_rect(renderer, bar_x + 3, bar_y + 3, bar_w, bar_h, 0, 0, 0, 90);

    /* Background */
    render_filled_rect(renderer, bar_x, bar_y, bar_w, bar_h, 14, 3, 3, 215);

    /* Outer border */
    render_rect_outline(renderer, bar_x,     bar_y,     bar_w,     bar_h,     150, 40, 40, 240);
    /* Inner border (inset 2px) */
    render_rect_outline(renderer, bar_x + 2, bar_y + 2, bar_w - 4, bar_h - 4, 70,  15, 15, 150);

    /* Chapter name (top-left of bar) */
    render_text(renderer, ch_name, bar_x + pad_x, bar_y + 6, 1, 200, 100, 100);

    /* "OBJECTIVE" label (right-aligned in the header row) */
    int label_x = bar_x + bar_w - render_text_width("OBJECTIVE", 1) - pad_x;
    render_text(renderer, "OBJECTIVE", label_x, bar_y + 6, 1, 130, 30, 30);

    /* Separator line */
    int sep_y = bar_y + 19;
    render_line(renderer, bar_x + 6, sep_y, bar_x + bar_w - 6, sep_y, 110, 28, 28, 210);

    /* Objective text (wrapped) */
    render_text_wrapped(renderer, obj_text,
                        bar_x + pad_x, sep_y + 5, max_tw,
                        1, line_h,
                        220, 145, 145);
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
