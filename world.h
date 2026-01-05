#ifndef WORLD_H
#define WORLD_H

#define CLEAR_SCREEN() printf("\033[2J\033[H")
#define MOVE_CURSOR() printf("\033[H")

struct SharedState;   
struct Walker;        

void allocate_world(struct SharedState *S);
void free_world(struct SharedState *S);

void initialize_world(struct SharedState *S);
void draw_world(struct SharedState *S);
void display_summary(struct SharedState *S);

#endif