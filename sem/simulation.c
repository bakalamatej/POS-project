#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <time.h>
#include "simulation.h"
#include "walker.h"

static int simulate_from(SharedState *S, Walker start)
{
    Walker w = start;

    for (int s = 0; s < S->max_steps; s++) {

        if (w.x == S->world_size/2 && w.y == S->world_size/2)
            return s;

        random_walk(S, &w);
    }
    return -1;
}

void* simulation_thread(void *arg)
{
    SharedState *S = arg;

    for (int r = 0; r < S->replications; r++) {

        pthread_mutex_lock(&S->lock);
        if (S->quit) {
            pthread_mutex_unlock(&S->lock);
            return NULL;
        }
        S->current_rep = r + 1;
        pthread_mutex_unlock(&S->lock);

        for (int y = 0; y < S->world_size; y++) {
            for (int x = 0; x < S->world_size; x++) {

                Walker start = { x, y };
                int steps = simulate_from(S, start);

                pthread_mutex_lock(&S->lock);

                if (S->quit) {
                    pthread_mutex_unlock(&S->lock);
                    return NULL;
                }

                if (steps != -1) {
                    S->success_count[y][x]++;
                    S->total_steps[y][x] += steps;
                }

                pthread_mutex_unlock(&S->lock);
            }
        }
    }

    pthread_mutex_lock(&S->lock);
    S->finished = true;
    pthread_mutex_unlock(&S->lock);

    return NULL;
}

void* walker_thread(void *arg)
{
    SharedState *S = arg;

    int steps = 0;

    struct timespec last, now;
    clock_gettime(CLOCK_MONOTONIC, &last);

    while (1) {

        pthread_mutex_lock(&S->lock);
        bool quit = S->quit;
        int max = S->max_steps;
        pthread_mutex_unlock(&S->lock);

        if (quit) return NULL;
        if (steps >= max) return NULL;

        clock_gettime(CLOCK_MONOTONIC, &now);

        long ms = (now.tv_sec - last.tv_sec)*1000 +
                  (now.tv_nsec - last.tv_nsec)/1000000;

        if (ms >= 300) {

            pthread_mutex_lock(&S->lock);
            random_walk(S, &S->walker);
            pthread_mutex_unlock(&S->lock);

            last = now;
            steps++;
        }

        usleep(1000);
    }
}