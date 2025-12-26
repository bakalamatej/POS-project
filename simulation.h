#ifndef SIMULATION_H
#define SIMULATION_H

#include "world.h"

int simulate_from(Walker start, int max_steps);
void run_simulation(World* world, int max_steps);

#endif