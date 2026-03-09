#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

/* String utilities */
char *utils_strdup(const char *src);
char *utils_strtrim(char *str);
int   utils_strncasecmp(const char *a, const char *b, size_t n);

/* File utilities */
char *utils_read_file(const char *path);
int   utils_file_exists(const char *path);

/* Random helpers */
int utils_rand_range(int min, int max);
void utils_srand_seed(unsigned int seed);

/* Text display */
void utils_print_slow(const char *text, unsigned int delay_ms);
void utils_clear_screen(void);
void utils_press_enter(void);

/* Numeric helpers */
int utils_clamp(int value, int min, int max);

#endif /* UTILS_H */
