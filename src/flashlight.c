#include "flashlight.h"

#include <math.h>
#include <string.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── Public API ────────────────────────────────────────────────────────── */

void flashlight_init(Flashlight *fl)
{
    if (!fl) return;
    memset(fl, 0, sizeof(*fl));
    fl->on    = false;
    fl->dir_x = 1.0f;   /* default: facing right */
    fl->dir_y = 0.0f;
}

void flashlight_toggle(Flashlight *fl)
{
    if (!fl) return;
    fl->on = !fl->on;
}

void flashlight_update(Flashlight *fl, float move_x, float move_y)
{
    if (!fl) return;

    /* Only update direction when the player is actually moving. */
    float len = sqrtf(move_x * move_x + move_y * move_y);
    if (len > 0.0f) {
        fl->dir_x = move_x / len;
        fl->dir_y = move_y / len;
    }
    /* If not moving, keep the last direction. */
}

/* ── Rendering ─────────────────────────────────────────────────────────── */

/* Fill a triangle (ax,ay)→(bx,by)→(cx,cy) using horizontal scanlines.
 * Uses the same algorithm as the reference implementation. */
static void fill_triangle(SDL_Renderer *renderer,
                           float ax, float ay,
                           float bx, float by,
                           float cx, float cy,
                           float screen_px, float screen_py,
                           float range)
{
    /* Sort vertices by y so ay <= by <= cy. */
    if (ay > by) { float t; t=ax;ax=bx;bx=t; t=ay;ay=by;by=t; }
    if (ay > cy) { float t; t=ax;ax=cx;cx=t; t=ay;ay=cy;cy=t; }
    if (by > cy) { float t; t=bx;bx=cx;cx=t; t=by;by=cy;cy=t; }

    float total_h = cy - ay;
    if (total_h < 1.0f) return;

    for (float y = ay; y <= cy; y += 1.0f) {
        bool second_half = (y > by);

        float seg_h = second_half ? (cy - by) : (by - ay);
        if (seg_h < 1.0f) seg_h = 1.0f;

        float alpha1 = (y - ay) / total_h;
        float alpha2 = second_half ? (y - by) / seg_h
                                   : (y - ay) / seg_h;

        float sx = ax + (cx - ax) * alpha1;
        float ex = second_half ? bx + (cx - bx) * alpha2
                               : ax + (bx - ax) * alpha2;

        if (sx > ex) { float t = sx; sx = ex; ex = t; }

        /* Euclidean distance-based brightness — brighter near the player. */
        float mid_x        = (sx + ex) * 0.5f;
        float dx_from_pl   = mid_x - screen_px;
        float dy_from_pl   = y     - screen_py;
        float dist         = sqrtf(dx_from_pl * dx_from_pl +
                                   dy_from_pl * dy_from_pl);
        float dist_factor  = 1.0f - (dist / (range > 0.0f ? range : 1.0f));
        if (dist_factor < 0.0f) dist_factor = 0.0f;
        if (dist_factor > 1.0f) dist_factor = 1.0f;

        Uint8 alpha = (Uint8)(180.0f * dist_factor);
        SDL_SetRenderDrawColor(renderer, 255, 240, 180, alpha);
        SDL_RenderLine(renderer, sx, y, ex, y);
    }
}

void flashlight_render(const Flashlight *fl, SDL_Renderer *renderer,
                       float player_x, float player_y,
                       float cam_x,    float cam_y,
                       int win_w,      int win_h)
{
    if (!fl || !renderer) return;

    /* Always draw the darkness overlay. */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, FLASHLIGHT_DARK_ALPHA);
    SDL_FRect full = { 0.0f, 0.0f, (float)win_w, (float)win_h };
    SDL_RenderFillRect(renderer, &full);

    if (!fl->on) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        return;
    }

    /* Build the cone as a triangle fan in screen space. */
    float base_angle = atan2f(fl->dir_y, fl->dir_x);
    float half_cone  = (FLASHLIGHT_ANGLE / 2.0f) * (float)(M_PI / 180.0);
    float range      = FLASHLIGHT_RANGE;

    /* Screen position of the player (cone apex). */
    float screen_px = player_x - cam_x;
    float screen_py = player_y - cam_y;

    /* Compute arc vertices. */
    float ray_sx[FLASHLIGHT_RAYS];
    float ray_sy[FLASHLIGHT_RAYS];

    for (int i = 0; i < FLASHLIGHT_RAYS; i++) {
        float t     = (float)i / (float)(FLASHLIGHT_RAYS - 1);
        float angle = base_angle - half_cone + t * (2.0f * half_cone);
        ray_sx[i] = screen_px + cosf(angle) * range;
        ray_sy[i] = screen_py + sinf(angle) * range;
    }

    /* Draw the cone as filled triangles (apex → ray[i] → ray[i+1]). */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < FLASHLIGHT_RAYS - 1; i++) {
        fill_triangle(renderer,
                      screen_px, screen_py,
                      ray_sx[i], ray_sy[i],
                      ray_sx[i + 1], ray_sy[i + 1],
                      screen_px, screen_py, range);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}
