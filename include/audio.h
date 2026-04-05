#ifndef AUDIO_H
#define AUDIO_H

/* ── Audio system ─────────────────────────────────────────────────────────
 * MP3 playback via SDL3's native audio API and the bundled dr_mp3 decoder.
 * No additional libraries beyond SDL3 are required.
 * ──────────────────────────────────────────────────────────────────────── */

/* Initialise the audio subsystem.  Returns 1 on success, 0 on failure. */
int  audio_init(void);

/* Shut down the audio subsystem and free all resources. */
void audio_cleanup(void);

/* Start playing an MP3 file.
 * Any currently playing audio is stopped first.
 * Plays once; does not loop. */
void audio_play_music(const char *path);

/* Stop music immediately. */
void audio_stop_music(void);

/* Returns 1 if music is currently playing, 0 otherwise. */
int  audio_is_playing(void);

/* Set playback volume.  vol is in the range 0–128 (128 = full volume). */
void audio_set_volume(int vol);

#endif /* AUDIO_H */
