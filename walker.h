#ifndef WALKER_H
#define WALKER_H

typedef struct Walker {
    int x;
    int y;
} Walker;

struct SharedState;          

void walker_init(Walker *w, int x, int y);
void random_walk(struct SharedState *S, Walker* w);

#endif