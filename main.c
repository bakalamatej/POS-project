#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "world.h"
#include "walker.h"
#include "simulation.h"

int main() {

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 500 * 1000000L;

    srand(time(NULL));

    World world;
    initialize_world(&world);

    int mode;
    int max_steps;

    printf("Select simulation mode:\n1 = interactive\n2 = summary\n");
    scanf("%d", &mode);

    if (mode == 1) {

        printf("Enter maximum number of steps: ");
        scanf("%d", &max_steps);

        Walker walker = { WORLD_SIZE/2, WORLD_SIZE/2 };

        for (int step = 0; step < max_steps; step++) {
            draw_world(walker);
            random_walk(&walker);
            nanosleep(&ts, NULL);
        }
    }
    else if (mode == 2) {

        printf("Enter maximum number of steps: ");
        scanf("%d", &max_steps);

        run_simulation(&world, max_steps);
        display_summary(&world);
    }
    else {
        printf("Invalid mode selection.\n");
    }

    return 0;
}