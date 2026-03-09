#ifndef UI_H
#define UI_H

#include "player.h"
#include "world.h"
#include "story.h"

/* ── Layout constants ──────────────────────────────────────────────────── */

#define UI_SCREEN_WIDTH  80   /* assumed terminal columns                 */
#define UI_SCREEN_HEIGHT 24   /* assumed terminal rows                    */

/* ── Colour / formatting helpers (ANSI escape codes) ─────────────────── */

#define UI_RESET   "\033[0m"
#define UI_BOLD    "\033[1m"
#define UI_DIM     "\033[2m"
#define UI_RED     "\033[31m"
#define UI_GREEN   "\033[32m"
#define UI_YELLOW  "\033[33m"
#define UI_BLUE    "\033[34m"
#define UI_MAGENTA "\033[35m"
#define UI_CYAN    "\033[36m"
#define UI_WHITE   "\033[37m"

/* ── API ───────────────────────────────────────────────────────────────── */

/* Initialise / tear down the UI layer.                                   */
void ui_init(void);
void ui_shutdown(void);

/* Screen management */
void ui_clear(void);
void ui_draw_separator(char ch);   /* prints a full-width line of ch     */

/* Title / splash */
void ui_show_title_screen(void);
void ui_show_game_over(void);

/* HUD – shown above the room description                                 */
void ui_draw_hud(const Player *player, const StoryState *state);

/* Room / location display */
void ui_show_location(const Location *loc);

/* Status bars */
void ui_draw_bar(const char *label, int value, int max, int bar_width);

/* Menu helpers */
int  ui_show_main_menu(void);      /* returns user selection (1-based)   */
int  ui_show_pause_menu(void);     /* returns 1=resume 2=save 3=quit     */
void ui_show_chapter_banner(int chapter);

/* Input */
void ui_get_string(const char *prompt, char *buf, int buf_size);
int  ui_get_choice(int min, int max);

/* Notification / message box */
void ui_message(const char *fmt, ...);
void ui_horror_flash(const char *text);   /* red flash for horror moments */

#endif /* UI_H */
