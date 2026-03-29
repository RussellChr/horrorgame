#ifndef FLASHLIGHT_H
#define FLASHLIGHT_H

#include <SDL3/SDL.h>
#include <stdbool.h>

/* ── Tuning constants ──────────────────────────────────────────────────── */

#define FLASHLIGHT_ANGLE      70.0f   /* total cone width in degrees        */
#define FLASHLIGHT_RANGE     420.0f   /* cone length in world/screen pixels */
#define FLASHLIGHT_RAYS       64      /* triangle-fan slices                */
#define FLASHLIGHT_DARK_ALPHA 210     /* darkness overlay opacity (0–255)   */

/* ── Flashlight ────────────────────────────────────────────────────────── */

typedef struct {
    bool  on;      /* true = light is on                          */
    float dir_x;   /* normalised cone direction X (-1 to +1)      */
    float dir_y;   /* normalised cone direction Y (-1 to +1)      */
} Flashlight;

/* Initialise with sensible defaults (off, facing right). */
void flashlight_init(Flashlight *fl);

/* Toggle the flashlight on/off. */
void flashlight_toggle(Flashlight *fl);

/* Update the cone direction to face (move_x, move_y).
 * Pass the player's current velocity or last input direction.
 * Direction is preserved when the player is stationary. */
void flashlight_update(Flashlight *fl, float move_x, float move_y);

/* Render a dark overlay with a lit cone cut-out.
 * player_x / player_y are world-space coordinates of the player.
 * cam_x  / cam_y  are the camera scroll offsets (world → screen: sx = wx - cam_x).
 * Call after the world and player have been drawn, before UI. */
void flashlight_render(const Flashlight *fl, SDL_Renderer *renderer,
                       float player_x, float player_y,
                       float cam_x,    float cam_y,
                       int win_w,      int win_h);

#endif /* FLASHLIGHT_H */
