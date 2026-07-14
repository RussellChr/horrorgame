#include <SDL3/SDL.h>
#include <errno.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "game.h"

static void set_working_directory_to_executable(void)
{
    char *base_path = SDL_GetBasePath();
    if (!base_path) return;

#ifdef _WIN32
    if (_chdir(base_path) != 0) {
#else
    if (chdir(base_path) != 0) {
#endif
        SDL_Log("Failed to set working directory to '%s': %s",
                base_path, strerror(errno));
    }
    SDL_free(base_path);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Project Yozora – A Horror Story",
        WINDOW_W, WINDOW_H, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* Scale the 1280×720 logical canvas to fill any window/fullscreen size,
       preserving aspect ratio with letterboxing. */
    SDL_SetRenderLogicalPresentation(renderer, WINDOW_W, WINDOW_H,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    /* Enable alpha blending globally. */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    /* Keep relative asset paths working when launched from app bundles. */
    set_working_directory_to_executable();

    Game *game = game_init(window, renderer);
    if (!game) {
        SDL_Log("game_init failed");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
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
    SDL_Quit();
    return 0;
}
