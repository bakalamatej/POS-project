#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

#include "simulation.h"
#include "walker.h"
#include "world.h"

void init_input()
{
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

void restore_input()
{
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

int main()
{
    srand(time(NULL));

    SharedState S;

    printf("World size: ");
    scanf("%d", &S.world_size);

    printf("Number of replications: ");
    scanf("%d", &S.replications);

    printf("Max steps (K): ");
    scanf("%d", &S.max_steps);

    double remaining = 1.0;

    /* ---- UP ---- */
    printf("Probability UP   (0–1): ");
    scanf("%lf", &S.prob.up);

    if (S.prob.up < 0) S.prob.up = 0;
    if (S.prob.up > remaining) S.prob.up = remaining;

    remaining -= S.prob.up;


    /* ---- DOWN ---- */
    if (remaining > 0.0) {

        printf("Probability DOWN (0–%.2f): ", remaining);
        scanf("%lf", &S.prob.down);

        if (S.prob.down < 0) S.prob.down = 0;
        if (S.prob.down > remaining) S.prob.down = remaining;

        remaining -= S.prob.down;

    } else {
        S.prob.down = 0.0;
    }


    /* ---- LEFT ---- */
    if (remaining > 0.0) {

        printf("Probability LEFT (0–%.2f): ", remaining);
        scanf("%lf", &S.prob.left);

        if (S.prob.left < 0) S.prob.left = 0;
        if (S.prob.left > remaining) S.prob.left = remaining;

        remaining -= S.prob.left;

    } else {
        S.prob.left = 0.0;
    }


    /* ---- RIGHT = remaining ---- */
    S.prob.right = remaining;

    printf("\nFinal probabilities:\n");
    printf("UP    = %.2f\n", S.prob.up);
    printf("DOWN  = %.2f\n", S.prob.down);
    printf("LEFT  = %.2f\n", S.prob.left);
    printf("RIGHT = %.2f\n", S.prob.right);


    printf("Select mode (1 interactive / 2 summary): ");
    scanf("%d", &S.mode);

    allocate_world(&S);
    initialize_world(&S);

    S.current_rep = 0;
    S.finished = false;
    S.quit = false;

    walker_init(&S.walker, S.world_size/2, S.world_size/2);

    pthread_mutex_init(&S.lock, NULL);

    pthread_t sim, walk;
    pthread_create(&sim, NULL, simulation_thread, &S);
    pthread_create(&walk, NULL, walker_thread, &S);

    printf("\nControls:\n1 interactive\n2 summary\nq quit\n\n");

    init_input();

    CLEAR_SCREEN();

    while (1) {

        pthread_mutex_lock(&S.lock);
        int mode = S.mode;
        pthread_mutex_unlock(&S.lock);

        if (mode == 1) 
            draw_world(&S);
        else 
            display_summary(&S);

        usleep(100000);

        int c = getchar();
        if (c == '1') {
            S.mode = 1;
            CLEAR_SCREEN();
        }
        if (c == '2') {
            S.mode = 2;
            CLEAR_SCREEN();
        }
        if (c == 'q') break;
    }

    restore_input();

    pthread_mutex_lock(&S.lock);
    S.quit = true;
    pthread_mutex_unlock(&S.lock);

    pthread_join(sim, NULL);
    pthread_join(walk, NULL);

    pthread_mutex_destroy(&S.lock);

    free_world(&S);

    return 0;
}