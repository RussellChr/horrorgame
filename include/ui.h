#ifndef UI_H
#define UI_H

#include <SDL3/SDL.h>
#include "player.h"

/* ── Button ───────────────────────────────────────────────────────────── */

typedef struct {
    SDL_FRect   rect;
    const char *text;
    int         is_hovered;
} Button;

void button_update_hover(Button *button, float mouse_x, float mouse_y);
int  button_is_clicked(Button *button, float mouse_x, float mouse_y);
void draw_button(SDL_Renderer *renderer, const Button *button);
void draw_button_menu(SDL_Renderer *r, const Button *b);

/* ── Slider ───────────────────────────────────────────────────────────── */

typedef struct {
    float x, y;     /* top-left of the track */
    float w, h;     /* width and height of the track */
    float value;    /* current value (min to max) */
    float min, max;
    int   focused;  /* 1 if this slider is currently focused */
} Slider;

void slider_init(Slider *s, float x, float y, float w, float h,
                 float min_val, float max_val, float value);
void slider_set_value(Slider *s, float value);
int  slider_handle_click(Slider *s, float mouse_x, float mouse_y);
void slider_render(SDL_Renderer *r, const Slider *s, const char *label);

/* ── HUD (heads-up display drawn over the game scene) ────────────────── */

void ui_draw_hud(SDL_Renderer *renderer, const Player *player);

/* ── Interaction prompt ───────────────────────────────────────────────── */

void ui_draw_interact_prompt(SDL_Renderer *renderer,
                             const char *label,
                             int screen_x, int screen_y);

#endif /* UI_H */
