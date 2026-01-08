#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <time.h>
#include "simulation.h"
#include "walker.h"
#include "ipc.h"

static int clamp_world_size(const SharedState *S)
{
    if (!S) return 0;
    return (S->world_size < IPC_MAX_WORLD) ? S->world_size : IPC_MAX_WORLD;
}

static void copy_summary_to_ipc(SharedState *S)
{
    if (!S || !S->ipc) return;
    int n = clamp_world_size(S);
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            S->ipc->total_steps[y][x] = S->total_steps[y][x];
            S->ipc->success_count[y][x] = S->success_count[y][x];
        }
    }
}

static void sync_progress_to_ipc(SharedState *S)
{
    if (!S || !S->ipc) return;
    int n = clamp_world_size(S);
    S->ipc->world_size = n;
    S->ipc->mode = S->mode;
    S->ipc->summary_view = S->summary_view;
    S->ipc->current_rep = S->current_rep;
    S->ipc->replications = S->replications;
    S->ipc->finished = S->finished ? 1 : 0;
}

static int simulate_from(SharedState *S, Walker start)
{
    Walker w = start;
    int center_x = S->world_size / 2;
    int center_y = S->world_size / 2;

    // Špeciálny prípad: Walker už začína v strede
    if (w.x == center_x && w.y == center_y)
        return 0;  // 0 krokov

    // Simuluj kroky
    for (int step = 1; step <= S->max_steps; step++) {
        random_walk(S, &w);  // Vykonaj krok
        
        // Skontroluj, či walker dosiahol stred
        if (w.x == center_x && w.y == center_y)
            return step;  // Úspech! Vráti počet krokov
    }
    
    return -1;  // Neúspech - walker nedosiahol stred za max_steps
}

void* simulation_thread(void *arg)
{
    SharedState *S = arg;

    for (int r = 0; r < S->replications; r++) {

        for (int y = 0; y < S->world_size; y++) {
            for (int x = 0; x < S->world_size; x++) {

                Walker start = { x, y };
                int steps = simulate_from(S, start);

                pthread_mutex_lock(&S->lock);

                if (steps != -1) {
                    S->success_count[y][x]++;
                    S->total_steps[y][x] += steps;
                }

                pthread_mutex_unlock(&S->lock);
            }
        }

        // Aktualizuj current_rep až PO dokončení celej replikácie
        pthread_mutex_lock(&S->lock);
        S->current_rep = r + 1;
        copy_summary_to_ipc(S);
        sync_progress_to_ipc(S);
        pthread_mutex_unlock(&S->lock);
    }

    pthread_mutex_lock(&S->lock);
    S->finished = true;
    copy_summary_to_ipc(S);
    sync_progress_to_ipc(S);
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
        bool finished = S->finished;
        int max = S->max_steps;
        pthread_mutex_unlock(&S->lock);

        if (finished) return NULL;
        if (steps >= max) return NULL;

        clock_gettime(CLOCK_MONOTONIC, &now);

        long ms = (now.tv_sec - last.tv_sec)*1000 +
                  (now.tv_nsec - last.tv_nsec)/1000000;

        if (ms >= 300) {

            pthread_mutex_lock(&S->lock);
            random_walk(S, &S->walker);
            if (S->ipc) {
                int n = clamp_world_size(S);
                int wx = S->walker.x;
                int wy = S->walker.y;
                if (wx >= n) wx %= n;
                if (wy >= n) wy %= n;
                S->ipc->walker_x = wx;
                S->ipc->walker_y = wy;
                S->ipc->mode = S->mode;
                S->ipc->summary_view = S->summary_view;
                S->ipc->finished = S->finished ? 1 : 0;
                S->ipc->world_size = n;
            }
            pthread_mutex_unlock(&S->lock);

            last = now;
            steps++;
        }

        usleep(1000);
    }
}