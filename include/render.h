#ifndef RENDER_H
#define RENDER_H

#include <SDL3/SDL.h>

/* ── Window dimensions (used by both game and world rendering) ────────── */
#define WINDOW_W 1280
#define WINDOW_H  720

/* ── Render primitives ────────────────────────────────────────────────── */

void render_filled_rect(SDL_Renderer *r, int x, int y, int w, int h,
                        Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha);

void render_rect_outline(SDL_Renderer *r, int x, int y, int w, int h,
                         Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha);

void render_line(SDL_Renderer *r, int x1, int y1, int x2, int y2,
                 Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha);

/* ── Bitmap font text rendering ───────────────────────────────────────── */
/* Font is 8x8 pixels; scale multiplies each pixel.                       */

void render_text(SDL_Renderer *r, const char *text, int x, int y,
                 int scale, Uint8 red, Uint8 green, Uint8 blue);

void render_text_centered(SDL_Renderer *r, const char *text,
                          int cx, int y, int scale,
                          Uint8 red, Uint8 green, Uint8 blue);

/* Draws text wrapped inside max_width pixels; returns number of lines. */
int  render_text_wrapped(SDL_Renderer *r, const char *text,
                         int x, int y, int max_width,
                         int scale, int line_h,
                         Uint8 red, Uint8 green, Uint8 blue);

int  render_text_width(const char *text, int scale);

/* ── Texture helpers ──────────────────────────────────────────────────── */

SDL_Texture *render_load_texture(SDL_Renderer *r, const char *path);
void         render_texture(SDL_Renderer *r, SDL_Texture *texture,
                            int x, int y, int w, int h);
void         render_texture_destroy(SDL_Texture *texture);

#endif /* RENDER_H */
