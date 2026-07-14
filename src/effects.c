#include "effects.h"
#include "game.h"
#include "player.h"
#include "world.h"
#include "camera.h"
#include "render.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── Flashlight cone constants ──────────────────────────────────────────── */
#define FL_NUM_RAYS  48
/* Slightly longer throw to keep contrast playable with darker ambient pass. */
#define FL_MAX_DIST  300.0f
#define FL_HALF_CONE (M_PI / 4.0)   /* half-cone: 45° (π/4 radians) each side */

/* ── Archive darkness constants ─────────────────────────────────────────── */
/* Number of triangle-fan segments for the ambient glow circle. */
#define DARK_AMBIENT_NUM_SEGS  48
/* Screen-space radius (pixels) of the ambient circle around the player. */
#define DARK_AMBIENT_RADIUS    120

/* ── Gas mask vignette constants ─────────────────────────────────────────── */
/* Number of triangle-fan segments for the gas mask vignette circle. */
#define GM_VIGNETTE_NUM_SEGS  64
/* Screen-space radius (pixels) of the visible circle when wearing the gas mask. */
#define GM_VIGNETTE_RADIUS    200

/* ── Gameplay vignette constants ─────────────────────────────────────────── */
#define GAME_VIGNETTE_NUM_SEGS  64
#define GAME_VIGNETTE_RADIUS    420


/* ── Shared helper: base angle from facing direction ─────────────────────── */

static double facing_to_angle(int dir)
{
    switch (dir) {
    case DIRECTION_EAST:  return  0.0;
    case DIRECTION_WEST:  return  M_PI;
    case DIRECTION_NORTH: return -M_PI / 2.0;
    case DIRECTION_SOUTH: return  M_PI / 2.0;
    default:              return  0.0;
    }
}


/* Render the flashlight cone when the flashlight is active.
 * Casts FL_NUM_RAYS rays within a 90-degree cone (±45°, i.e. π/4 radians,
 * from the player's facing direction) and draws the lit area as a
 * triangle fan using additive blending. */
void render_flashlight_beam(Game *game)
{
    if (!game->flashlight_active) return;
    Player   *p   = game->player;
    Location *loc = world_get_location(game->world, p->current_location_id);
    if (!loc) return;

    /* World-space origin: vertical centre of the player collider */
    float ox = p->x;
    float oy = p->y - (float)PLAYER_COLLIDER_OFFSET_Y
                    + (float)PLAYER_COLLIDER_H * 0.5f;

    double base_angle = facing_to_angle(p->current_direction);

    /* Build a triangle-fan in screen space:
     *   verts[0]          = ray origin (player centre)
     *   verts[1..N+1]     = hit points for each ray */
    SDL_Vertex verts[FL_NUM_RAYS + 2];
    int        indices[FL_NUM_RAYS * 3];

    int sx0 = camera_to_screen_x(&game->camera, ox);
    int sy0 = camera_to_screen_y(&game->camera, oy);
    verts[0].position.x  = (float)sx0;
    verts[0].position.y  = (float)sy0;
    verts[0].color.r     = 1.0f;
    verts[0].color.g     = 1.0f;
    verts[0].color.b     = 0.8f;
    verts[0].color.a     = 0.60f;
    verts[0].tex_coord.x = 0.0f;
    verts[0].tex_coord.y = 0.0f;

    for (int i = 0; i <= FL_NUM_RAYS; i++) {
        double angle = base_angle - FL_HALF_CONE
                       + (2.0 * FL_HALF_CONE * i) / FL_NUM_RAYS;
        float dx    = (float)cos(angle);
        float dy    = (float)sin(angle);
        float hx    = ox + dx * FL_MAX_DIST;
        float hy    = oy + dy * FL_MAX_DIST;

        int sx = camera_to_screen_x(&game->camera, hx);
        int sy = camera_to_screen_y(&game->camera, hy);

        /* Fade alpha toward the cone edges: 1.0 at centre, 0.0 at ±45°.
         * Formula: edge = 1 - |2*(i/N) - 1|, mapping [0,N] → [0,1,0]. */
        float edge = 1.0f - 2.0f * fabsf((float)i / FL_NUM_RAYS - 0.5f);

        verts[i + 1].position.x  = (float)sx;
        verts[i + 1].position.y  = (float)sy;
        verts[i + 1].color.r     = 1.0f;
        verts[i + 1].color.g     = 1.0f;
        verts[i + 1].color.b     = 0.7f;
        verts[i + 1].color.a     = 0.34f * edge;
        verts[i + 1].tex_coord.x = 0.0f;
        verts[i + 1].tex_coord.y = 0.0f;
    }

    for (int i = 0; i < FL_NUM_RAYS; i++) {
        indices[i * 3 + 0] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = i + 2;
    }

    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_ADD);
    SDL_RenderGeometry(game->renderer, NULL,
                       verts, FL_NUM_RAYS + 2,
                       indices, FL_NUM_RAYS * 3);
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
}


/* Renders the archive room darkness effect (location 0 only).
 *
 * Builds a light-mask on the dark_overlay render-target:
 *   – Starts fully black (opaque).
 *   – Adds a small, very dim ambient circle around the player so nearby
 *     shapes are faintly visible.
 *   – If the flashlight is active, adds a bright white triangle fan in the
 *     player's facing direction (same raycast geometry as the beam).
 *
 * The finished mask is then composited onto the screen with
 * SDL_BLENDMODE_MOD, which multiplies every screen pixel by the mask colour:
 *   black mask  → screen pixel becomes black (hidden)
 *   white mask  → screen pixel is unchanged (fully revealed)
 *
 * The additive flashlight beam drawn afterwards by render_flashlight_beam()
 * adds the warm yellowish glow on top of the revealed scene.
 */
void render_archive_darkness(Game *game)
{
    if (!game->dark_overlay) return;
    if (game->player->current_location_id != 0) return;

    SDL_Renderer *r   = game->renderer;
    Player       *p   = game->player;
    Location     *loc = world_get_location(game->world, p->current_location_id);

    /* World-space origin: vertical centre of the player collider */
    float ox = p->x;
    float oy = p->y - (float)PLAYER_COLLIDER_OFFSET_Y
                    + (float)PLAYER_COLLIDER_H * 0.5f;
    int sx0 = camera_to_screen_x(&game->camera, ox);
    int sy0 = camera_to_screen_y(&game->camera, oy);

    /* ── Build the light mask ──────────────────────────────────────────── */
    SDL_SetRenderTarget(r, game->dark_overlay);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);

    /* Ambient glow: a dim warm circle centred on the player.
     * Centre brightness 0.45 – nearby shapes are visible but colours are
     * heavily muted.  Falls off to black at the edge so vision is clearly
     * limited to a small radius. */
    {
        SDL_Vertex av[DARK_AMBIENT_NUM_SEGS + 2];
        int        ai[DARK_AMBIENT_NUM_SEGS * 3];

        av[0].position.x  = (float)sx0;
        av[0].position.y  = (float)sy0;
        av[0].color.r     = 0.45f;
        av[0].color.g     = 0.42f;
        av[0].color.b     = 0.35f;
        av[0].color.a     = 1.0f;
        av[0].tex_coord.x = 0.0f;
        av[0].tex_coord.y = 0.0f;

        for (int i = 0; i <= DARK_AMBIENT_NUM_SEGS; i++) {
            double angle = (2.0 * M_PI * i) / DARK_AMBIENT_NUM_SEGS;
            av[i + 1].position.x  = (float)sx0
                                    + DARK_AMBIENT_RADIUS * (float)cos(angle);
            av[i + 1].position.y  = (float)sy0
                                    + DARK_AMBIENT_RADIUS * (float)sin(angle);
            av[i + 1].color.r     = 0.0f;
            av[i + 1].color.g     = 0.0f;
            av[i + 1].color.b     = 0.0f;
            av[i + 1].color.a     = 1.0f;
            av[i + 1].tex_coord.x = 0.0f;
            av[i + 1].tex_coord.y = 0.0f;
        }
        for (int i = 0; i < DARK_AMBIENT_NUM_SEGS; i++) {
            ai[i * 3 + 0] = 0;
            ai[i * 3 + 1] = i + 1;
            ai[i * 3 + 2] = i + 2;
        }
        SDL_RenderGeometry(r, NULL,
                           av, DARK_AMBIENT_NUM_SEGS + 2,
                           ai, DARK_AMBIENT_NUM_SEGS * 3);
    }

    /* Flashlight cone: reveal the scene in the illuminated area.
     * Uses the same raycasting geometry as render_flashlight_beam() but
     * draws in bright white so MOD blend fully uncovers the room texture. */
    if (game->flashlight_active && loc) {
        double base_angle = facing_to_angle(p->current_direction);

        SDL_Vertex fv[FL_NUM_RAYS + 2];
        int        fi[FL_NUM_RAYS * 3];

        fv[0].position.x  = (float)sx0;
        fv[0].position.y  = (float)sy0;
        fv[0].color.r     = 1.0f;
        fv[0].color.g     = 1.0f;
        fv[0].color.b     = 1.0f;
        fv[0].color.a     = 1.0f;
        fv[0].tex_coord.x = 0.0f;
        fv[0].tex_coord.y = 0.0f;

        for (int i = 0; i <= FL_NUM_RAYS; i++) {
            double angle = base_angle - FL_HALF_CONE
                           + (2.0 * FL_HALF_CONE * i) / FL_NUM_RAYS;
            float dx   = (float)cos(angle);
            float dy   = (float)sin(angle);
            float hx   = ox + dx * FL_MAX_DIST;
            float hy   = oy + dy * FL_MAX_DIST;

            float edge = 1.0f - 2.0f * fabsf((float)i / FL_NUM_RAYS - 0.5f);

            fv[i + 1].position.x  = (float)camera_to_screen_x(&game->camera, hx);
            fv[i + 1].position.y  = (float)camera_to_screen_y(&game->camera, hy);
            fv[i + 1].color.r     = 1.0f;
            fv[i + 1].color.g     = 1.0f;
            fv[i + 1].color.b     = 0.9f;
            fv[i + 1].color.a     = 0.55f * edge;
            fv[i + 1].tex_coord.x = 0.0f;
            fv[i + 1].tex_coord.y = 0.0f;
        }
        for (int i = 0; i < FL_NUM_RAYS; i++) {
            fi[i * 3 + 0] = 0;
            fi[i * 3 + 1] = i + 1;
            fi[i * 3 + 2] = i + 2;
        }
        SDL_RenderGeometry(r, NULL,
                           fv, FL_NUM_RAYS + 2,
                           fi, FL_NUM_RAYS * 3);
    }

    /* ── Apply the mask to the screen via multiply blend ──────────────── */
    SDL_SetRenderTarget(r, NULL);
    SDL_SetTextureBlendMode(game->dark_overlay, SDL_BLENDMODE_MOD);
    SDL_FRect dst = { 0.0f, 0.0f, (float)WINDOW_W, (float)WINDOW_H };
    SDL_RenderTexture(r, game->dark_overlay, NULL, &dst);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
}


/* Renders a radial vignette effect when the gas mask is active.
 *
 * Uses the same dark_overlay render-target approach as render_archive_darkness():
 *   – Fills the overlay with opaque black.
 *   – Draws a smooth white circle (triangle fan, full-white centre fading to
 *     black at the edge) centred on the player using additive blending.
 *
 * The overlay is then composited onto the screen with SDL_BLENDMODE_MOD:
 *   black mask → hidden, white mask → fully visible.
 * This restricts the player's view to a circle around their character.
 */
void render_gasmask_vignette(Game *game)
{
    if (!game->gasmask_active) return;
    if (!game->dark_overlay)   return;

    SDL_Renderer *r = game->renderer;
    Player       *p = game->player;

    /* World-space origin: vertical centre of the player collider */
    float ox = p->x;
    float oy = p->y - (float)PLAYER_COLLIDER_OFFSET_Y
                    + (float)PLAYER_COLLIDER_H * 0.5f;
    int sx0 = camera_to_screen_x(&game->camera, ox);
    int sy0 = camera_to_screen_y(&game->camera, oy);

    /* ── Build the vignette mask ────────────────────────────────────────── */
    SDL_SetRenderTarget(r, game->dark_overlay);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    /* Draw a smooth gradient circle: white centre → black edge.
     * ADD blend on the (black) overlay so only the circle region
     * becomes visible after the MOD composite step. */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
    {
        SDL_Vertex av[GM_VIGNETTE_NUM_SEGS + 2];
        int        ai[GM_VIGNETTE_NUM_SEGS * 3];

        /* Centre vertex: fully white (reveals the scene directly under it) */
        av[0].position.x  = (float)sx0;
        av[0].position.y  = (float)sy0;
        av[0].color.r     = 1.0f;
        av[0].color.g     = 1.0f;
        av[0].color.b     = 1.0f;
        av[0].color.a     = 1.0f;
        av[0].tex_coord.x = 0.0f;
        av[0].tex_coord.y = 0.0f;

        /* Edge vertices: fully black (hides the scene at the perimeter) */
        for (int i = 0; i <= GM_VIGNETTE_NUM_SEGS; i++) {
            double angle = (2.0 * M_PI * i) / GM_VIGNETTE_NUM_SEGS;
            av[i + 1].position.x  = (float)sx0
                                    + GM_VIGNETTE_RADIUS * (float)cos(angle);
            av[i + 1].position.y  = (float)sy0
                                    + GM_VIGNETTE_RADIUS * (float)sin(angle);
            av[i + 1].color.r     = 0.0f;
            av[i + 1].color.g     = 0.0f;
            av[i + 1].color.b     = 0.0f;
            av[i + 1].color.a     = 1.0f;
            av[i + 1].tex_coord.x = 0.0f;
            av[i + 1].tex_coord.y = 0.0f;
        }

        for (int i = 0; i < GM_VIGNETTE_NUM_SEGS; i++) {
            ai[i * 3 + 0] = 0;
            ai[i * 3 + 1] = i + 1;
            ai[i * 3 + 2] = i + 2;
        }

        SDL_RenderGeometry(r, NULL,
                           av, GM_VIGNETTE_NUM_SEGS + 2,
                           ai, GM_VIGNETTE_NUM_SEGS * 3);
    }

    /* ── Apply the mask to the screen via multiply blend ───────────────── */
    SDL_SetRenderTarget(r, NULL);
    SDL_SetTextureBlendMode(game->dark_overlay, SDL_BLENDMODE_MOD);
    SDL_FRect dst = { 0.0f, 0.0f, (float)WINDOW_W, (float)WINDOW_H };
    SDL_RenderTexture(r, game->dark_overlay, NULL, &dst);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
}

void render_screen_vignette(Game *game)
{
    if (!game || !game->dark_overlay) return;
    if (!game->player) return;
    if (game->state != GAME_STATE_PLAYING &&
        game->state != GAME_STATE_DIALOGUE &&
        game->state != GAME_STATE_PAUSE) return;
    if (game->gasmask_active) return;

    SDL_Renderer *r = game->renderer;
    float ox = game->player->x;
    float oy = game->player->y - (float)PLAYER_COLLIDER_OFFSET_Y
                            + (float)PLAYER_COLLIDER_H * 0.5f;
    float sx0 = (float)camera_to_screen_x(&game->camera, ox);
    float sy0 = (float)camera_to_screen_y(&game->camera, oy);

    SDL_SetRenderTarget(r, game->dark_overlay);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
    {
        SDL_Vertex av[GAME_VIGNETTE_NUM_SEGS + 2];
        int        ai[GAME_VIGNETTE_NUM_SEGS * 3];

        av[0].position.x  = sx0;
        av[0].position.y  = sy0;
        av[0].color.r     = 0.94f;
        av[0].color.g     = 0.94f;
        av[0].color.b     = 0.94f;
        av[0].color.a     = 1.0f;
        av[0].tex_coord.x = 0.0f;
        av[0].tex_coord.y = 0.0f;

        for (int i = 0; i <= GAME_VIGNETTE_NUM_SEGS; i++) {
            double angle = (2.0 * M_PI * i) / GAME_VIGNETTE_NUM_SEGS;
            av[i + 1].position.x  = sx0
                                    + GAME_VIGNETTE_RADIUS * (float)cos(angle);
            av[i + 1].position.y  = sy0
                                    + GAME_VIGNETTE_RADIUS * (float)sin(angle);
            av[i + 1].color.r     = 0.0f;
            av[i + 1].color.g     = 0.0f;
            av[i + 1].color.b     = 0.0f;
            av[i + 1].color.a     = 1.0f;
            av[i + 1].tex_coord.x = 0.0f;
            av[i + 1].tex_coord.y = 0.0f;
        }

        for (int i = 0; i < GAME_VIGNETTE_NUM_SEGS; i++) {
            ai[i * 3 + 0] = 0;
            ai[i * 3 + 1] = i + 1;
            ai[i * 3 + 2] = i + 2;
        }

        SDL_RenderGeometry(r, NULL,
                           av, GAME_VIGNETTE_NUM_SEGS + 2,
                           ai, GAME_VIGNETTE_NUM_SEGS * 3);
    }

    SDL_SetRenderTarget(r, NULL);
    SDL_SetTextureBlendMode(game->dark_overlay, SDL_BLENDMODE_MOD);
    SDL_FRect dst = { 0.0f, 0.0f, (float)WINDOW_W, (float)WINDOW_H };
    SDL_RenderTexture(r, game->dark_overlay, NULL, &dst);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
}
