#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <stdio.h>
#include "utils.h"

// Pomocné utility: prepínanie terminálu do raw módu a bezpečné kopírovanie reťazcov.
// Zapne raw mód terminálu a uloží pôvodné nastavenia.
void enable_raw_mode(struct termios *orig)
{
    if (tcgetattr(STDIN_FILENO, orig) == -1) return;
    
    struct termios raw = *orig;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Obnoví pôvodné nastavenia terminálu.
void disable_raw_mode(const struct termios *orig)
{
    if (orig) tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
}

// Bezpečne skopíruje reťazec s garantovaným ukončením nulou.
void safe_strcpy(char *dest, const char *src, size_t size)
{
    if (!dest || !src) return;
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
}
