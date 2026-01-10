#ifndef UTILS_H
#define UTILS_H

#define _POSIX_C_SOURCE 200809L

#include <termios.h>
#include <stdbool.h>

// Terminal utilities
void enable_raw_mode(struct termios *orig);
void disable_raw_mode(const struct termios *orig);

// String utilities
void safe_strcpy(char *dest, const char *src, size_t size);

#endif // UTILS_H
