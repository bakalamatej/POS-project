#ifndef SIMULATION_H
#define SIMULATION_H

#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdbool.h>
#include "walker.h"

struct IPCShared;

typedef struct {
    double up;
    double down;
    double left;
    double right;
} Probabilities;

typedef struct SharedState {

    int world_size;
    int replications;
    int max_steps;

    int current_rep;

    int **total_steps;
    int **success_count;
    
    bool use_obstacles;
    int **obstacles;  // 1 = obstacle, 0 = free

    Walker walker;

    int mode;   // 1 interactive / 2 summary
    int summary_view; // 0 average steps, 1 probability
    bool finished;

    Probabilities prob;

    pthread_mutex_t lock;

    struct IPCShared *ipc; // ukazovatel na zdieľanú pamäť (nastaví server)

} SharedState;

void* simulation_thread(void *arg);
void* walker_thread(void *arg);

#endif