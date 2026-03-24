#include <SDL3/SDL.h>
#include <string.h>
#include "game.h"

int main(int argc, char *argv[])
{
    /* Determine asset base path: -a <path> flag overrides SDL_GetBasePath() */
    char asset_base_path[512] = {0};
    const char *flag_override = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) {
            if (i + 1 < argc)
                flag_override = argv[i + 1];
            break;
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    /* Resolve asset base path after SDL_Init so SDL_GetBasePath() is available */
    if (flag_override) {
        size_t len = strlen(flag_override);
        snprintf(asset_base_path, sizeof(asset_base_path), "%s", flag_override);
        /* Ensure the path ends with a separator */
        if (len > 0 && asset_base_path[len - 1] != '/'
                    && asset_base_path[len - 1] != '\\') {
            if (len + 1 < sizeof(asset_base_path)) {
                asset_base_path[len]     = '/';
                asset_base_path[len + 1] = '\0';
            }
        }
    } else {
        char *base = SDL_GetBasePath();
        if (base) {
            snprintf(asset_base_path, sizeof(asset_base_path), "%s", base);
            SDL_free(base);
        }
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

    /* Enable alpha blending globally. */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    Game *game = game_init(window, renderer, asset_base_path);
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

