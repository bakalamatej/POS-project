#include "simulation.h"
#include "walker.h"

int simulate_from(Walker start, int max_steps) {
    Walker walker = start;

    for (int step = 0; step < max_steps; step++) {

        if (walker.x == WORLD_SIZE/2 && walker.y == WORLD_SIZE/2)
            return step;

        random_walk(&walker);
    }

    return -1;
}

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

            world->total_steps[i][j] =
                successful_reps > 0 ? step_sum / successful_reps : 0;

            world->success_count[i][j] = successful_reps;
        }
    }
}