#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#endif

/* ── String utilities ──────────────────────────────────────────────────── */

char *utils_strdup(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src) + 1;
    char *copy = malloc(len);
    if (copy) memcpy(copy, src, len);
    return copy;
}

char *utils_strtrim(char *str) {
    if (!str) return NULL;
    /* trim leading whitespace */
    while (isspace((unsigned char)*str)) str++;
    if (*str == '\0') return str;
    /* trim trailing whitespace */
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    return str;
}

int utils_strncasecmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int ca = tolower((unsigned char)a[i]);
        int cb = tolower((unsigned char)b[i]);
        if (ca != cb) return ca - cb;
        if (ca == '\0') return 0;
    }
    return 0;
}

/* ── File utilities ────────────────────────────────────────────────────── */

char *utils_read_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    if (size < 0) { fclose(fp); return NULL; }

    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(fp); return NULL; }

    size_t read = fread(buf, 1, (size_t)size, fp);
    buf[read] = '\0';
    fclose(fp);
    return buf;
}

int utils_file_exists(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

/* ── Random helpers ────────────────────────────────────────────────────── */

void utils_srand_seed(unsigned int seed) {
    srand(seed);
}

int utils_rand_range(int min, int max) {
    if (max <= min) return min;
    return min + rand() % (max - min + 1);
}

/* ── Text display ──────────────────────────────────────────────────────── */

void utils_print_slow(const char *text, unsigned int delay_ms) {
    for (; *text; text++) {
        putchar(*text);
        fflush(stdout);
#ifdef _WIN32
        Sleep(delay_ms);
#else
        usleep(delay_ms * 1000u);
#endif
    }
}

void utils_clear_screen(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void utils_press_enter(void) {
    printf("\n[Press ENTER to continue]\n");
    fflush(stdout);
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/* ── Numeric helpers ───────────────────────────────────────────────────── */

int utils_clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}
