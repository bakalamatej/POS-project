#include <stdlib.h>
#include "walker.h"
#include "simulation.h"

void walker_init(Walker *w, int x, int y)
{
    w->x = x;
    w->y = y;
}

void random_walk(SharedState *S, Walker* w)
{
    double p_up    = S->prob.up;
    double p_down  = p_up   + S->prob.down;
    double p_left  = p_down + S->prob.left;

    double r = (double)rand() / RAND_MAX;

    int new_x = w->x;
    int new_y = w->y;

    if (r < p_up)
        new_y = (w->y - 1 + S->world_size) % S->world_size;
    else if (r < p_down)
        new_y = (w->y + 1) % S->world_size;
    else if (r < p_left)
        new_x = (w->x - 1 + S->world_size) % S->world_size;
    else
        new_x = (w->x + 1) % S->world_size;

    if (!S->use_obstacles || S->obstacles[new_y][new_x] == 0) {
        w->x = new_x;
        w->y = new_y;
    }
}