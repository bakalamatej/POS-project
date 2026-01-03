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

    printf("Use obstacles? (1 yes / 0 no): ");
    int use_obs;
    scanf("%d", &use_obs);
    S.use_obstacles = (use_obs == 1);

    if (S.use_obstacles) {
        int size_from_file = get_world_size_from_obstacles("obstacles.txt");
        if (size_from_file > 0) {
            S.world_size = size_from_file;
            printf("World size loaded from obstacles.txt: %d\n", S.world_size);
        } else {
            printf("Error: Could not load obstacles.txt. Exiting.\n");
            return 1;
        }
    } else {
        printf("World size: ");
        scanf("%d", &S.world_size);
    }

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

    if (S.use_obstacles) {
        if (!load_obstacles(&S, "obstacles.txt")) {
            printf("Failed to load obstacles. Exiting.\n");
            free_world(&S);
            return 1;
        }
    }

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

    while (1) {
        pthread_mutex_lock(&S.lock);
        int mode = S.mode;
        pthread_mutex_unlock(&S.lock);

        if (mode == 1)
            draw_world(&S);
        else
            display_summary(&S);

       

        printf("[1] - interaktívny režim\n");
        printf("[2] - sumárny režim\n");
        printf("[ESC] - ukončiť simuláciu\n");

        usleep(100000);

        int c = getchar();
        if (c == '1') S.mode = 1;
        if (c == '2') S.mode = 2;
        if (c == 27) break; 
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