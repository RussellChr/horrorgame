#include "ui.h"
#include "utils.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

void ui_init(void) {
    /* Nothing to initialise for a terminal-based UI. */
}

void ui_shutdown(void) {
    /* Nothing to tear down. */
}

/* ── Screen management ─────────────────────────────────────────────────── */

void ui_clear(void) {
    utils_clear_screen();
}

void ui_draw_separator(char ch) {
    for (int i = 0; i < UI_SCREEN_WIDTH; i++) putchar(ch);
    putchar('\n');
}

/* ── Title screen ──────────────────────────────────────────────────────── */

void ui_show_title_screen(void) {
    ui_clear();
    ui_draw_separator('=');
    printf("\n");
    printf("             %s%sP A P E R   L I L Y%s\n",
           UI_RED, UI_BOLD, UI_RESET);
    printf("                  A Horror Story\n\n");
    printf("   \"Some doors should never be opened.\"\n");
    printf("   \"Some children should not be found.\"\n");
    printf("\n");
    ui_draw_separator('=');
    printf("\n");
}

void ui_show_game_over(void) {
    ui_clear();
    ui_draw_separator('=');
    printf("\n%s%s                   GAME  OVER%s\n\n",
           UI_RED, UI_BOLD, UI_RESET);
    ui_draw_separator('=');
    printf("\n");
    utils_press_enter();
}

/* ── HUD ───────────────────────────────────────────────────────────────── */

void ui_draw_hud(const Player *player, const StoryState *state) {
    if (!player) return;
    ui_draw_separator('-');
    printf(" %s%-20s%s", UI_BOLD, player->name, UI_RESET);
    printf("  HP:%3d  SAN:%3d  COU:%3d",
           player->health, player->sanity, player->courage);
    if (state) {
        const char *chapters[] = {
            "Prologue", "Ch.I", "Ch.II", "Ch.III", "Finale"
        };
        int ch = state->current_chapter;
        if (ch >= 0 && ch < CHAPTER_COUNT)
            printf("  [%s]", chapters[ch]);
    }
    printf("\n");
    ui_draw_separator('-');
}

/* ── Location display ──────────────────────────────────────────────────── */

void ui_show_location(const Location *loc) {
    if (!loc) return;
    printf("\n%s%s%s\n", UI_BOLD, loc->name, UI_RESET);
    ui_draw_separator('~');
    printf("%s\n", loc->visited ? (loc->atmosphere[0]
                                    ? loc->atmosphere
                                    : loc->description)
                                 : loc->description);
    if (loc->exit_count > 0) {
        printf("\n%sExits:%s", UI_DIM, UI_RESET);
        for (int i = 0; i < loc->exit_count; i++) {
            if (loc->exits[i].locked)
                printf(" [%s%s (locked)%s]",
                       UI_RED, loc->exits[i].direction, UI_RESET);
            else
                printf(" [%s]", loc->exits[i].direction);
        }
        printf("\n");
    }
}

/* ── Status bar ────────────────────────────────────────────────────────── */

void ui_draw_bar(const char *label, int value, int max, int bar_width) {
    if (max <= 0) return;
    int filled = (value * bar_width) / max;
    filled = utils_clamp(filled, 0, bar_width);

    printf("%-10s [", label);
    for (int i = 0; i < bar_width; i++)
        putchar(i < filled ? '#' : '.');
    printf("] %3d/%3d\n", value, max);
}

/* ── Menus ─────────────────────────────────────────────────────────────── */

int ui_show_main_menu(void) {
    ui_show_title_screen();
    printf("  1. New Game\n");
    printf("  2. Load Game\n");
    printf("  3. Quit\n\n");
    return ui_get_choice(1, 3);
}

int ui_show_pause_menu(void) {
    printf("\n── PAUSED ──────────────────────────────────\n");
    printf("  1. Resume\n");
    printf("  2. Save Game\n");
    printf("  3. Quit to Menu\n");
    return ui_get_choice(1, 3);
}

void ui_show_chapter_banner(int chapter) {
    printf("\n");
    ui_draw_separator('*');
    if (chapter == 0)
        printf("%s%s                   PROLOGUE%s\n", UI_BOLD, UI_YELLOW, UI_RESET);
    else
        printf("%s%s                   CHAPTER %d%s\n",
               UI_BOLD, UI_YELLOW, chapter, UI_RESET);
    ui_draw_separator('*');
    printf("\n");
}

/* ── Input helpers ─────────────────────────────────────────────────────── */

void ui_get_string(const char *prompt, char *buf, int buf_size) {
    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buf, buf_size, stdin)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
    }
}

int ui_get_choice(int min, int max) {
    int sel = min;
    printf("Choice (%d-%d): ", min, max);
    fflush(stdout);
    if (scanf("%d", &sel) != 1) sel = min;
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    return utils_clamp(sel, min, max);
}

/* ── Notifications ─────────────────────────────────────────────────────── */

void ui_message(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("\n%s>>> %s", UI_CYAN, UI_RESET);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

void ui_horror_flash(const char *text) {
    printf("\n%s%s%s%s\n", UI_RED, UI_BOLD, text, UI_RESET);
    utils_press_enter();
}
