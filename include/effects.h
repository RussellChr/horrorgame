#ifndef EFFECTS_H
#define EFFECTS_H

#include "game.h"

/* ── Visual effect renderers ─────────────────────────────────────────────
 *
 * All three functions read from the Game context (player position, facing
 * direction, active flags) and write directly to the SDL renderer.
 *
 * render_flashlight_beam  – additive warm-yellow cone in the player's
 *                            facing direction (drawn when flashlight_active).
 * render_archive_darkness – light-mask overlay for location 0; restricts
 *                            vision to a small ambient circle + flashlight
 *                            cone via SDL_BLENDMODE_MOD.
 * render_gasmask_vignette – circular vignette that limits the field of view
 *                            when the gas mask is active (any location).
 */

void render_flashlight_beam(Game *game);
void render_archive_darkness(Game *game);
void render_gasmask_vignette(Game *game);

#endif /* EFFECTS_H */
