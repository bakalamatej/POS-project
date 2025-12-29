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

    if (r < p_up)
        w->y = (w->y - 1 + S->world_size) % S->world_size;
    else if (r < p_down)
        w->y = (w->y + 1) % S->world_size;
    else if (r < p_left)
        w->x = (w->x - 1 + S->world_size) % S->world_size;
    else
        w->x = (w->x + 1) % S->world_size;
}