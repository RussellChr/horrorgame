#include "game.h"
#include "ui.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("  -n <name>    Player character name (default: \"Stranger\")\n");
    printf("  -a <path>    Path to assets directory (default: assets)\n");
    printf("  -s           Enable slow (typewriter) text\n");
    printf("  -c           Disable ANSI colour output\n");
    printf("  -h           Show this help\n");
}

int main(int argc, char *argv[]) {
    GameConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.slow_text    = 0;
    cfg.text_delay_ms = 30;
    cfg.ansi_colour  = 1;
    strncpy(cfg.asset_path, "assets", sizeof(cfg.asset_path) - 1);

    char player_name[64] = "Stranger";
    int  show_help = 0;

    /* Minimal argument parsing */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            show_help = 1;
        } else if (strcmp(argv[i], "-s") == 0) {
            cfg.slow_text = 1;
        } else if (strcmp(argv[i], "-c") == 0) {
            cfg.ansi_colour = 0;
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            strncpy(player_name, argv[++i], sizeof(player_name) - 1);
        } else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            strncpy(cfg.asset_path, argv[++i], sizeof(cfg.asset_path) - 1);
        }
    }

    if (show_help) {
        print_usage(argv[0]);
        return 0;
    }

    utils_srand_seed((unsigned int)time(NULL));

    /* Show title and main menu */
    ui_init();
    int choice = ui_show_main_menu();
    if (choice == 3) {
        printf("Goodbye.\n");
        ui_shutdown();
        return 0;
    }

    if (choice == 1) {
        /* Prompt for name */
        ui_get_string("Enter your name: ", player_name, sizeof(player_name));
        if (player_name[0] == '\0')
            strncpy(player_name, "Stranger", sizeof(player_name) - 1);
    }

    /* Initialise and run the game */
    GameState *gs = game_init(player_name, &cfg);
    if (!gs) {
        fprintf(stderr, "Failed to initialise game.\n");
        return 1;
    }

    game_run(gs);
    game_shutdown(gs);

    return 0;
}
