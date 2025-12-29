#ifndef WORLD_H
#define WORLD_H

struct SharedState;   
struct Walker;        

void allocate_world(struct SharedState *S);
void free_world(struct SharedState *S);

void initialize_world(struct SharedState *S);
void draw_world(struct SharedState *S);
void display_summary(struct SharedState *S);

#endif