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

/* ── HUD (heads-up display drawn over the game scene) ────────────────── */

void ui_draw_hud(SDL_Renderer *renderer, const Player *player);

/* ── Interaction prompt ───────────────────────────────────────────────── */

void ui_draw_interact_prompt(SDL_Renderer *renderer,
                             const char *label,
                             int screen_x, int screen_y);

#endif /* UI_H */
