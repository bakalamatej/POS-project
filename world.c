#include <stdio.h>
#include <stdlib.h>
#include "simulation.h"
#include "world.h"

#define CLEAR_SCREEN() printf("\033[H\033[J")

void allocate_world(SharedState *S)
{
    S->total_steps = malloc(S->world_size * sizeof(int*));
    S->success_count = malloc(S->world_size * sizeof(int*));

    for (int i = 0; i < S->world_size; i++) {
        S->total_steps[i] = calloc(S->world_size, sizeof(int));
        S->success_count[i] = calloc(S->world_size, sizeof(int));
    }
}

void free_world(SharedState *S)
{
    for (int i = 0; i < S->world_size; i++) {
        free(S->total_steps[i]);
        free(S->success_count[i]);
    }
    free(S->total_steps);
    free(S->success_count);
}

void initialize_world(SharedState *S)
{
    for (int i = 0; i < S->world_size; i++)
        for (int j = 0; j < S->world_size; j++) {
            S->total_steps[i][j] = 0;
            S->success_count[i][j] = 0;
        }
}

void draw_world(SharedState *S)
{
    CLEAR_SCREEN();

    for (int i = 0; i < S->world_size; i++) {
        for (int j = 0; j < S->world_size; j++) {

            if (i == S->world_size/2 && j == S->world_size/2)
                printf("C ");
            else if (i == S->walker.y && j == S->walker.x)
                printf("W ");
            else
                printf(". ");
        }
        printf("\n");
    }
}

void display_summary(SharedState *S)
{
    CLEAR_SCREEN();

    printf("Replication %d / %d\n\n",
           S->current_rep, S->replications);

    printf("Average number of steps:\n");

    for (int y = 0; y < S->world_size; y++) {
        for (int x = 0; x < S->world_size; x++) {
            if (S->success_count[y][x] > 0)
                printf("%3d ",
                    S->total_steps[y][x] / S->success_count[y][x]);
            else
                printf(" -- ");
        }
        printf("\n");
    }

    printf("\nProbability of reaching the center:\n");

    for (int y = 0; y < S->world_size; y++) {
        for (int x = 0; x < S->world_size; x++) {
            printf("%3d%% ", 
                (S->success_count[y][x] * 100) / S->replications);
        }
        printf("\n");
    }
}