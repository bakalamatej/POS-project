#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "world.h"

// ANSI escape sekvencie na ovl�danie konzoly
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

void random_walk(Walker* walker) {
    int direction = rand() % 4;
    switch (direction) {
    case 0: walker->x = (walker->x + 1) % WORLD_SIZE; break; 
    case 1: walker->x = (walker->x - 1 + WORLD_SIZE) % WORLD_SIZE; break; 
    case 2: walker->y = (walker->y + 1) % WORLD_SIZE; break; 
    case 3: walker->y = (walker->y - 1 + WORLD_SIZE) % WORLD_SIZE; break; 
    }
}

int simulate_from(Walker start, int max_steps) {
    Walker walker = start;
    for (int step = 0; step < max_steps; step++) {
        if (walker.x == WORLD_SIZE/2 && walker.y == WORLD_SIZE/2) {
            return step; 
        }
        random_walk(&walker);
    }
    return -1; 
}

// Spustenie simul�cie a v�po�et priemern�ch krokov a �spe�nosti
void run_simulation(World* world, int max_steps) {
    for (int i = 0; i < WORLD_SIZE; i++) {
        for (int j = 0; j < WORLD_SIZE; j++) {
            Walker start = { i, j };
            int successful_reps = 0;
            int step_sum = 0;

            for (int rep = 0; rep < REPLICATIONS; rep++) {
                int steps = simulate_from(start, max_steps);
                if (steps != -1) {
                    successful_reps++;
                    step_sum += steps;
                }
            }

            world->total_steps[i][j] = successful_reps > 0 ? step_sum / successful_reps : 0;
            world->success_count[i][j] = successful_reps;
        }
    }
}

void draw_world(Walker walker) {
    CLEAR_SCREEN();
    for (int i = 0; i < WORLD_SIZE; i++) {
        for (int j = 0; j < WORLD_SIZE; j++) {
            if (i == walker.x && j == walker.y) {
                printf("W ");  
            }
            else {
                printf(". "); 
            }
        }
        printf("\n");
    }
    


}

void display_summary(const World* world) {
    printf("Priemern� po�et krokov na dosiahnutie stredu:\n");
    for (int i = 0; i < WORLD_SIZE; i++) {
        for (int j = 0; j < WORLD_SIZE; j++) {
            printf("%3d ", world->total_steps[i][j]);
        }
        printf("\n");
    }

    printf("\nPravdepodobnos� dosiahnutia stredu (v %%):\n");
    for (int i = 0; i < WORLD_SIZE; i++) {
        for (int j = 0; j < WORLD_SIZE; j++) {
            printf("%3d%% ", (world->success_count[i][j] * 100) / REPLICATIONS);
        }
        printf("\n");
    }
}

