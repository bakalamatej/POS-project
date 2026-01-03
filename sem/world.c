#include <stdio.h>
#include <stdlib.h>
#include "simulation.h"
#include "world.h"

#define CLEAR_SCREEN() printf("\033[H\033[J")

void allocate_world(SharedState *S)
{
    S->total_steps = malloc(S->world_size * sizeof(int*));
    S->success_count = malloc(S->world_size * sizeof(int*));
    S->obstacles = malloc(S->world_size * sizeof(int*));

    for (int i = 0; i < S->world_size; i++) {
        S->total_steps[i] = calloc(S->world_size, sizeof(int));
        S->success_count[i] = calloc(S->world_size, sizeof(int));
        S->obstacles[i] = calloc(S->world_size, sizeof(int));
    }
}

void free_world(SharedState *S)
{
    for (int i = 0; i < S->world_size; i++) {
        free(S->total_steps[i]);
        free(S->success_count[i]);
        free(S->obstacles[i]);
    }
    free(S->total_steps);
    free(S->success_count);
    free(S->obstacles);
}

void initialize_world(SharedState *S)
{
    for (int i = 0; i < S->world_size; i++)
        for (int j = 0; j < S->world_size; j++) {
            S->total_steps[i][j] = 0;
            S->success_count[i][j] = 0;
            S->obstacles[i][j] = 0;
        }
}

int get_world_size_from_obstacles(const char* filename)
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        return -1;
    }

    int size = -1;
    if (fscanf(file, "%d", &size) != 1 || size <= 0) {
        fclose(file);
        return -1;
    }

    fclose(file);
    return size;
}

int load_obstacles(SharedState *S, const char* filename)
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open obstacles file '%s'.\n", filename);
        return 0; // File not found
    }

    int size = 0;
    if (fscanf(file, "%d", &size) != 1 || size <= 0) {
        printf("Error: Invalid or missing size in obstacles file '%s'.\n", filename);
        fclose(file);
        return 0;
    }

    if (size != S->world_size) {
        printf("Error: Obstacles file size %d does not match configured world size %d.\n", size, S->world_size);
        fclose(file);
        return 0;
    }

    // Read obstacles matrix
    for (int i = 0; i < S->world_size; i++) {
        for (int j = 0; j < S->world_size; j++) {
            if (fscanf(file, "%d", &S->obstacles[i][j]) != 1) {
                printf("Error: Invalid obstacles file format at position [%d][%d].\n", i, j);
                fclose(file);
                initialize_world(S); // Reset obstacles
                return 0;
            }
        }
    }

    // Ensure center is not blocked (where walker starts)
    int center = S->world_size / 2;
    if (S->obstacles[center][center] == 1) {
        printf("Warning: Obstacle at center position removed.\n");
        S->obstacles[center][center] = 0;
    }

    fclose(file);
    printf("Obstacles loaded successfully from '%s'.\n", filename);
    return 1; // Success
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
            else if (S->use_obstacles && S->obstacles[i][j] == 1)
                printf("# ");
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
            if (S->use_obstacles && S->obstacles[y][x]) {
                printf(" ###");
            } else if (S->success_count[y][x] > 0) {
                printf("%3d ",
                    S->total_steps[y][x] / S->success_count[y][x]);
            } else {
                printf(" -- ");
            }
        }
        printf("\n");
    }

    printf("\nProbability of reaching the center:\n");

    for (int y = 0; y < S->world_size; y++) {
        for (int x = 0; x < S->world_size; x++) {
            if (S->use_obstacles && S->obstacles[y][x]) {
                printf(" ###");
            } else {
                printf("%3d%% ", 
                    (S->success_count[y][x] * 100) / S->replications);
            }
        }
        printf("\n");
    }

    if (S->use_obstacles) {
        printf("\nLegend: ### obstacle / impassable cell\n");
    }
}