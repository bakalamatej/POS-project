#ifndef WORLD_H
#define WORLD_H

#define WORLD_SIZE 10
#define REPLICATIONS 1000

typedef struct {
    int x;
    int y;
} Walker;

typedef struct {
    int total_steps[WORLD_SIZE][WORLD_SIZE];
    int success_count[WORLD_SIZE][WORLD_SIZE];
} World;

void initialize_world(World* world);
void draw_world(Walker walker);
void display_summary(const World* world);

#endif