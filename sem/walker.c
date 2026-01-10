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

    // Urči základný smer pohybu (bez wrap-around)
    if (r < p_up)
        new_y = w->y - 1;
    else if (r < p_down)
        new_y = w->y + 1;
    else if (r < p_left)
        new_x = w->x - 1;
    else
        new_x = w->x + 1;
  
    // Pre svet BEZ prekážok: aplikuj wrap-around
    if (!S->use_obstacles) {
        new_x = (new_x + S->world_size) % S->world_size;
        new_y = (new_y + S->world_size) % S->world_size;
        
        // Skontroluj prekážku (aj keď by nemali byť žiadne)
        if (S->obstacles[new_y][new_x] == 0) {
            w->x = new_x;
            w->y = new_y;
        }
    } else {
        // Pre svet S PREKÁŽKAMI: zostane na mieste pri hranici/prekážke
        if (new_x >= 0 && new_x < S->world_size && 
            new_y >= 0 && new_y < S->world_size &&
            S->obstacles[new_y][new_x] == 0) {
            w->x = new_x;
            w->y = new_y;
        }
        // Inak zostane na pôvodnej pozícii
    }
}