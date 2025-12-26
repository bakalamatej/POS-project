#include <stdlib.h>
#include "walker.h"
#include "world.h"

void random_walk(Walker* walker) {
    int direction = rand() % 4;

    switch (direction) {
        case 0: walker->x = (walker->x + 1) % WORLD_SIZE; break;
        case 1: walker->x = (walker->x - 1 + WORLD_SIZE) % WORLD_SIZE; break;
        case 2: walker->y = (walker->y + 1) % WORLD_SIZE; break;
        case 3: walker->y = (walker->y - 1 + WORLD_SIZE) % WORLD_SIZE; break;
    }
}