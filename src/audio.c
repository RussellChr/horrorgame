/* ── audio.c ───────────────────────────────────────────────────────────────
 * MP3 playback via SDL3's native audio API and the bundled single-header
 * dr_mp3 decoder.  No SDL3_mixer required.
 * ──────────────────────────────────────────────────────────────────────── */

/* Include dr_mp3 implementation exactly once */
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#include "audio.h"
#include <SDL3/SDL.h>

static SDL_AudioStream *s_stream  = NULL; /* current playback stream   */
static int              s_playing = 0;    /* 1 while audio is queued   */
static float            s_gain    = 1.0f; /* 0.0–1.0 volume multiplier */

int audio_init(void)
{
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("audio_init: SDL_InitSubSystem(AUDIO) failed: %s",
                SDL_GetError());
        return 0;
    }
    return 1;
}

void audio_cleanup(void)
{
    audio_stop_music();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void audio_play_music(const char *path)
{
    if (!path) return;

    audio_stop_music();

    /* Decode the entire MP3 to interleaved f32 PCM */
    drmp3_uint32 channels    = 0;
    drmp3_uint32 sample_rate = 0;
    drmp3_uint64 frame_count = 0;
    float *pcm = drmp3_open_file_and_read_pcm_frames_f32(
        path, &channels, &sample_rate, &frame_count, NULL);
    if (!pcm) {
        SDL_Log("audio_play_music: failed to decode \"%s\"", path);
        return;
    }

    SDL_AudioSpec spec;
    spec.format   = SDL_AUDIO_F32;
    spec.channels = (int)channels;
    spec.freq     = (int)sample_rate;

    /* Open a logical device and create a stream bound to it in one call.
     * NULL callback = push mode: we feed data, SDL plays it. */
    s_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!s_stream) {
        SDL_Log("audio_play_music: SDL_OpenAudioDeviceStream failed: %s",
                SDL_GetError());
        drmp3_free(pcm, NULL);
        return;
    }

    SDL_SetAudioStreamGain(s_stream, s_gain);

    /* Queue all decoded PCM data then signal end-of-stream */
    int bytes = (int)(frame_count * channels * sizeof(float));
    SDL_PutAudioStreamData(s_stream, pcm, bytes);
    SDL_FlushAudioStream(s_stream);
    drmp3_free(pcm, NULL);

    /* New streams start paused in SDL3 – resume the underlying device */
    SDL_ResumeAudioStreamDevice(s_stream);

    s_playing = 1;
}

void audio_stop_music(void)
{
    if (s_stream) {
        SDL_DestroyAudioStream(s_stream);
        s_stream = NULL;
    }
    s_playing = 0;
}

int audio_is_playing(void)
{
    if (!s_playing || !s_stream) return 0;

    /* SDL_GetAudioStreamQueued returns bytes still in the input queue.
     * Once 0, SDL has consumed everything and playback is done. */
    if (SDL_GetAudioStreamQueued(s_stream) > 0) return 1;

    SDL_DestroyAudioStream(s_stream);
    s_stream  = NULL;
    s_playing = 0;
    return 0;
}

void audio_set_volume(int vol)
{
    /* vol is 0–128 (MIX_MAX_VOLUME scale); convert to 0.0–1.0 gain */
    s_gain = (float)vol / 128.0f;
    if (s_stream)
        SDL_SetAudioStreamGain(s_stream, s_gain);
}
