#include <stdio.h>
#include "world.h"

#define MOVE_CURSOR(x, y) printf("\033[%d;%dH", (x), (y))
#define CLEAR_SCREEN() printf("\033[H\033[J")

void initialize_world(World* world) {
    for (int i = 0; i < WORLD_SIZE; i++) {
        for (int j = 0; j < WORLD_SIZE; j++) {
            world->total_steps[i][j] = 0;
            world->success_count[i][j] = 0;
        }
    }
}

void draw_world(Walker walker) {
    CLEAR_SCREEN();

    for (int i = 0; i < WORLD_SIZE; i++) {
        for (int j = 0; j < WORLD_SIZE; j++) {

            if (i == walker.x && j == walker.y)
                printf("W ");
            else
                printf(". ");
        }
        printf("\n");
    }
}

void display_summary(const World* world) {

    printf("Average number of steps:\n");
    for (int i = 0; i < WORLD_SIZE; i++) {
        for (int j = 0; j < WORLD_SIZE; j++) {
            printf("%3d ", world->total_steps[i][j]);
        }
        printf("\n");
    }

    printf("\nProbability of reaching the center:\n");
    for (int i = 0; i < WORLD_SIZE; i++) {
        for (int j = 0; j < WORLD_SIZE; j++) {
            printf("%3d%% ",
                   (world->success_count[i][j] * 100) / REPLICATIONS);
        }
        printf("\n");
    }
}