#ifndef WORLD_H
#define WORLD_H

#define WORLD_SIZE 11  // Ve�kos� sveta
#define REPLICATIONS 1000  // Po�et replik�ci� pre sum�rny m�d

// �trukt�ra reprezentuj�ca poz�ciu chodca
typedef struct Walker {
    int x;
    int y;
} Walker;

// �trukt�ra sveta, ktor� obsahuje inform�cie o po�te krokov a �spe�nosti dosiahnutia stredu
typedef struct World{
    int total_steps[WORLD_SIZE][WORLD_SIZE];    // Priemern� po�et krokov
    int success_count[WORLD_SIZE][WORLD_SIZE];  // Po�et �spe�n�ch pokusov
} World;

// Funkcie na pr�cu so svetom a simul�ciou
void initialize_world(World* world);
void random_walk(Walker* walker);
int simulate_from(Walker start, int max_steps);
void run_simulation(World* world, int max_steps);
void draw_world(Walker walker);
void display_summary(const World* world);

#endif

