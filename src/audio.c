#include "audio.h"
#include <SDL3/SDL.h>

#ifdef HAVE_SDL3_MIXER
#include <SDL3_mixer/SDL_mixer.h>

static Mix_Music *s_music = NULL;

int audio_init(void)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        SDL_Log("audio_init: SDL_InitSubSystem(AUDIO) failed: %s",
                SDL_GetError());
        return 0;
    }

    if (Mix_OpenAudio(0, NULL) < 0) {
        SDL_Log("audio_init: Mix_OpenAudio failed: %s", SDL_GetError());
        return 0;
    }

    return 1;
}

void audio_cleanup(void)
{
    if (s_music) {
        Mix_HaltMusic();
        Mix_FreeMusic(s_music);
        s_music = NULL;
    }
    Mix_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void audio_play_music(const char *path)
{
    if (!path) return;

    /* Stop and free any previously loaded music */
    if (s_music) {
        Mix_HaltMusic();
        Mix_FreeMusic(s_music);
        s_music = NULL;
    }

    s_music = Mix_LoadMUS(path);
    if (!s_music) {
        SDL_Log("audio_play_music: Mix_LoadMUS(\"%s\") failed: %s",
                path, SDL_GetError());
        return;
    }

    /* Play once (loops = 0 plays the music one time, no repeat) */
    if (Mix_PlayMusic(s_music, 0) < 0) {
        SDL_Log("audio_play_music: Mix_PlayMusic failed: %s", SDL_GetError());
    }
}

void audio_stop_music(void)
{
    Mix_HaltMusic();
    if (s_music) {
        Mix_FreeMusic(s_music);
        s_music = NULL;
    }
}

int audio_is_playing(void)
{
    return Mix_PlayingMusic();
}

void audio_set_volume(int vol)
{
    Mix_VolumeMusic(vol);
}

#else /* HAVE_SDL3_MIXER not defined – stub implementations */

int  audio_init(void)          { return 0; }
void audio_cleanup(void)       {}
void audio_play_music(const char *path) { (void)path; }
void audio_stop_music(void)    {}
int  audio_is_playing(void)    { return 0; }
void audio_set_volume(int vol) { (void)vol; }

#endif /* HAVE_SDL3_MIXER */
