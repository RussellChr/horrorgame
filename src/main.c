#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "game.h"

#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    /* Change the working directory to where the exe lives so that relative
     * asset paths ("assets/…", "maps/…") work regardless of how or from
     * where the executable was launched (double-click, shortcut, CLI, etc.). */
    char *base = SDL_GetBasePath();
    if (base) {
        if (chdir(base) != 0)
            SDL_Log("Warning: could not change working directory to '%s' – "
                    "asset loading may fail", base);
        SDL_free(base);
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    /* Initialise SDL3_image with PNG and JPG support.
     * IMG_Init MUST be called before any IMG_LoadTexture / IMG_Load call,
     * otherwise every load fails with "file format not supported". */
    int img_flags = IMG_INIT_PNG | IMG_INIT_JPG;
    if ((IMG_Init(img_flags) & img_flags) != img_flags) {
        SDL_Log("IMG_Init failed to enable PNG/JPG support: %s",
                IMG_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Project Yozora – A Horror Story",
        WINDOW_W, WINDOW_H, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    /* Scale the 1280×720 logical canvas to fill any window/fullscreen size,
       preserving aspect ratio with letterboxing. */
    SDL_SetRenderLogicalPresentation(renderer, WINDOW_W, WINDOW_H,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    /* Enable alpha blending globally. */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    Game *game = game_init(window, renderer);
    if (!game) {
        SDL_Log("game_init failed");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    /* ── Main loop ─────────────────────────────────────────────────────── */
    while (game->running && game->state != GAME_STATE_QUIT) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT)
                game->running = 0;
            game_handle_event(game, &event);
        }

        /* Clear */
        SDL_SetRenderDrawColor(renderer, 8, 6, 12, 255);
        SDL_RenderClear(renderer);

        /* Update + render */
        game_update(game);
        game_render(game);

        SDL_RenderPresent(renderer);
        SDL_Delay(16); /* ~60 FPS cap */
    }

    game_cleanup(game);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}

