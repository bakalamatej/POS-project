#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>  
#include "world.h"

int main() {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 500 * 1000000L; // 500 ms
    srand(time(NULL));
    World world;
    initialize_world(&world);

    int mode;
    int max_steps;

    printf("Zvolte mod simulacie \n1 = interaktivny \n2 = sumarny \n");
    scanf("%d", &mode);

    if (mode == 1) {
        printf("Zadajte maximalny pocet krokov: ");
        scanf("%d", &max_steps);
        Walker walker = { WORLD_SIZE / 2, WORLD_SIZE / 2 };
        for (int step = 0; step < max_steps; step++) {
            draw_world(walker);
            random_walk(&walker);
            printf("%d", step+1);
            nanosleep(&ts, NULL);  
        }
    }
    else if (mode == 2) {
        printf("Zadajte maximalny pocet krokov: ");
        scanf("%d", &max_steps);
        run_simulation(&world, max_steps);
        display_summary(&world);
    }
    else {
        printf("Neplatna volba modu.\n");
    }

    return 0;
}

