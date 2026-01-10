#ifndef WORLD_H
#define WORLD_H

#define CLEAR_SCREEN() printf("\033[2J\033[H")
#define MOVE_CURSOR() printf("\033[H")

struct SharedState;   
struct Walker;        

void allocate_world(struct SharedState *S);
void free_world(struct SharedState *S);

void initialize_world(struct SharedState *S);
int get_world_size_from_obstacles(const char* filename);
int load_obstacles(struct SharedState *S, const char* filename);
int save_simulation_results(struct SharedState *S, const char* filename);
int load_previous_simulation(struct SharedState *S, const char* filename);

#endif