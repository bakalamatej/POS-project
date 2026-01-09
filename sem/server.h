#ifndef SERVER_H
#define SERVER_H

#include "simulation.h"

typedef struct ServerConfig {
    int world_size;
    int replications;
    int max_steps;
    double prob_up;
    double prob_down;
    double prob_left;
    double prob_right;
    char obstacles_file[256];
    char output_file[256];
    char resume_file[256];
} ServerConfig;

// Spusti serverovu cast simulacie (povodna logika z main.c)
int server_run(const ServerConfig *config);

#endif // SERVER_H
