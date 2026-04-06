#ifndef VIDEO_H
#define VIDEO_H

#include <SDL3/SDL.h>

/* ── VideoPlayer ──────────────────────────────────────────────────────────
 * Plays a video file (MOV/MP4/…) using FFmpeg for decoding and SDL3 for
 * rendering and audio output.  A single frame texture is kept up-to-date
 * each call to video_player_update(); call video_player_render() to blit
 * it full-screen.
 * ----------------------------------------------------------------------- */

typedef struct VideoPlayer VideoPlayer;

/* Open a video file and prepare for playback.
 * Returns NULL on failure (file not found, unsupported codec, …). */
VideoPlayer *video_player_open(SDL_Renderer *renderer, const char *path);

/* Advance playback by dt seconds: decode video frames and push audio data.
 * Should be called once per game frame. */
void video_player_update(VideoPlayer *vp, float dt);

/* Blit the current video frame full-screen (1280×720). */
void video_player_render(VideoPlayer *vp, SDL_Renderer *renderer);

/* Returns 1 when the video has finished playing. */
int  video_player_is_done(const VideoPlayer *vp);

/* Free all resources. Safe to call with NULL. */
void video_player_close(VideoPlayer *vp);

#endif /* VIDEO_H */
