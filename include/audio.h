#ifndef AUDIO_H
#define AUDIO_H

/* ── Audio system ─────────────────────────────────────────────────────────
 * Thin wrapper around SDL3_mixer for music playback.
 * All functions are safe to call when SDL3_mixer is not compiled in;
 * they become no-ops and audio_is_playing() always returns 0.
 * ──────────────────────────────────────────────────────────────────────── */

/* Initialise the audio subsystem.  Returns 1 on success, 0 on failure. */
int  audio_init(void);

/* Shut down the audio subsystem and free all resources. */
void audio_cleanup(void);

/* Start playing a music file (MP3 / OGG / WAV …).
 * Any currently playing music is stopped first.
 * Plays once; does not loop. */
void audio_play_music(const char *path);

/* Stop music immediately. */
void audio_stop_music(void);

/* Returns 1 if music is currently playing, 0 otherwise. */
int  audio_is_playing(void);

/* Set playback volume.  vol is in the range 0–128 (MIX_MAX_VOLUME). */
void audio_set_volume(int vol);

#endif /* AUDIO_H */
