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

static void clear_screen(void)
{
    system("clear");
}

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
    int menu_obs = 0;
    int prob_mode = 0;

    /* KROK 1/5 – konfigurácia sveta a prekážok */
    while (1) {
        clear_screen();
        printf("========================================\n");
        printf("      RANDOM WALKER SIMULÁCIA\n");
        printf("========================================\n");
        printf("Krok 1/5 – konfigurácia sveta\n");
        printf("----------------------------------------\n");
        printf("[1] Použiť prekážky z obstacles.txt\n");
        printf("[2] Zadať veľkosť sveta manuálne\n");
        printf("----------------------------------------\n");
        printf("Zvoľte možnosť (1/2): ");

        if (scanf("%d", &menu_obs) == 1 && (menu_obs == 1 || menu_obs == 2)) {
            while (getchar() != '\n');
            break;
        }

        printf("\nNeplatná voľba. Stlačte Enter a skúste znova...");
        while (getchar() != '\n');
    }

    S.use_obstacles = (menu_obs == 1);

    if (S.use_obstacles) {
        int size_from_file = get_world_size_from_obstacles("obstacles.txt");
        if (size_from_file > 0) {
            S.world_size = size_from_file;
        } else {
            clear_screen();
            printf("Chyba: Nepodarilo sa načítať obstacles.txt. Končím.\n");
            return 1;
        }
    } else {
        while (1) {
            clear_screen();
            printf("========================================\n");
            printf("      RANDOM WALKER SIMULÁCIA\n");
            printf("========================================\n");
            printf("Krok 1/5 – veľkosť sveta\n");
            printf("----------------------------------------\n");
            printf("Zadajte veľkosť sveta: ");

            if (scanf("%d", &S.world_size) == 1 && S.world_size > 0) {
                while (getchar() != '\n');
                break;
            }

            printf("\nNeplatná veľkosť. Zadajte kladné číslo. Stlačte Enter a skúste znova...");
            while (getchar() != '\n');
        }
    }

    /* KROK 2/5 – počet replikácií */
    while (1) {
        clear_screen();
        printf("========================================\n");
        printf("      RANDOM WALKER SIMULÁCIA\n");
        printf("========================================\n");
        printf("Krok 2/5 – počet replikácií\n");
        printf("----------------------------------------\n");
        printf("Zadajte počet replikácií: ");

        if (scanf("%d", &S.replications) == 1 && S.replications > 0) {
            while (getchar() != '\n');
            break;
        }

        printf("\nNeplatný počet. Zadajte kladné číslo. Stlačte Enter a skúste znova...");
        while (getchar() != '\n');
    }

    /* KROK 3/5 – maximálny počet krokov */
    while (1) {
        clear_screen();
        printf("========================================\n");
        printf("      RANDOM WALKER SIMULÁCIA\n");
        printf("========================================\n");
        printf("Krok 3/5 – maximálny počet krokov\n");
        printf("----------------------------------------\n");
        printf("Zadajte maximálny počet krokov (K): ");

        if (scanf("%d", &S.max_steps) == 1 && S.max_steps > 0) {
            while (getchar() != '\n');
            break;
        }

        printf("\nNeplatný počet. Zadajte kladné číslo. Stlačte Enter a skúste znova...");
        while (getchar() != '\n');
    }

    /* KROK 4/5 – pravdepodobnosti pohybu */
    while (1) {
        clear_screen();
        printf("========================================\n");
        printf("      RANDOM WALKER SIMULÁCIA\n");
        printf("========================================\n");
        printf("Krok 4/5 – pravdepodobnosti pohybu\n");
        printf("----------------------------------------\n");
        printf("[1] Zadať pravdepodobnosti manuálne\n");
        printf("[2] Použiť predvolené hodnoty (0.25 pre každý smer)\n");
        printf("Zvoľte možnosť (1/2): ");

        if (scanf("%d", &prob_mode) == 1 && (prob_mode == 1 || prob_mode == 2)) {
            while (getchar() != '\n');
            break;
        }

        printf("\nNeplatná voľba. Zadajte 1 alebo 2. Stlačte Enter a skúste znova...");
        while (getchar() != '\n');
    }

    if (prob_mode == 2) {
        S.prob.up = S.prob.down = S.prob.left = S.prob.right = 0.25;
    } else {
        double remaining = 1.0;

        while (1) {
            clear_screen();
            printf("========================================\n");
            printf("      RANDOM WALKER SIMULÁCIA\n");
            printf("========================================\n");
            printf("Krok 4/5 – pravdepodobnosť UP\n");
            printf("----------------------------------------\n");
            printf("Zadajte pravdepodobnosť UP   (0–1): ");

            if (scanf("%lf", &S.prob.up) == 1 && S.prob.up >= 0.0 && S.prob.up <= 1.0) {
                while (getchar() != '\n');
                if (S.prob.up > remaining) S.prob.up = remaining;
                remaining -= S.prob.up;
                break;
            }

            printf("\nNeplatný vstup. Stlačte Enter a skúste znova...");
            while (getchar() != '\n');
        }

        if (remaining > 0.0) {
            while (1) {
                clear_screen();
                printf("========================================\n");
                printf("      RANDOM WALKER SIMULÁCIA\n");
                printf("========================================\n");
                printf("Krok 4/5 – pravdepodobnosť DOWN\n");
                printf("----------------------------------------\n");
                printf("Zadajte pravdepodobnosť DOWN (0–%.2f): ", remaining);

                if (scanf("%lf", &S.prob.down) == 1 && S.prob.down >= 0.0 && S.prob.down <= remaining) {
                    while (getchar() != '\n');
                    remaining -= S.prob.down;
                    break;
                }

                printf("\nNeplatný vstup. Stlačte Enter a skúste znova...");
                while (getchar() != '\n');
            }
        } else {
            S.prob.down = 0.0;
        }

        if (remaining > 0.0) {
            while (1) {
                clear_screen();
                printf("========================================\n");
                printf("      RANDOM WALKER SIMULÁCIA\n");
                printf("========================================\n");
                printf("Krok 4/5 – pravdepodobnosť LEFT\n");
                printf("----------------------------------------\n");
                printf("Zadajte pravdepodobnosť LEFT (0–%.2f): ", remaining);

                if (scanf("%lf", &S.prob.left) == 1 && S.prob.left >= 0.0 && S.prob.left <= remaining) {
                    while (getchar() != '\n');
                    remaining -= S.prob.left;
                    break;
                }

                printf("\nNeplatný vstup. Stlačte Enter a skúste znova...");
                while (getchar() != '\n');
            }
        } else {
            S.prob.left = 0.0;
        }

        S.prob.right = remaining;
    }

    /* KROK 5/5 – voľba režimu */
    while (1) {
        clear_screen();
        printf("========================================\n");
        printf("      RANDOM WALKER SIMULÁCIA\n");
        printf("========================================\n");
        printf("Krok 5/5 – režim simulácie\n");
        printf("----------------------------------------\n");
        printf("[1] Interaktívny režim\n");
        printf("[2] Sumárny režim\n");
        printf("Zvoľte režim (1/2): ");

        if (scanf("%d", &S.mode) == 1 && (S.mode == 1 || S.mode == 2)) {
            while (getchar() != '\n');
            break;
        }

        printf("\nNeplatná voľba. Zadajte 1 alebo 2. Stlačte Enter a skúste znova...");
        while (getchar() != '\n');
    }

    clear_screen();
    printf("========================================\n");
    printf("  NASTAVENIE SIMULÁCIE DOKONČENÉ\n");
    printf("========================================\n");

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

    printf("\nPočas simulácie môžete prepínať režimy alebo ukončiť simuláciu:\n");
    printf("[1] - interaktívny režim\n");
    printf("[2] - sumárny režim\n");
    printf("[ESC] - ukončiť simuláciu\n\n");

    init_input();

    while (1) {
        pthread_mutex_lock(&S.lock);
        int mode = S.mode;
        pthread_mutex_unlock(&S.lock);

        if (mode == 1)
            draw_world(&S);
        else
            display_summary(&S);

       

        printf("[1] - interactive mode\n");
        printf("[2] - summary mode\n");
        printf("[ESC] - quit simulation\n");

        usleep(100000);

        int c = getchar();
        if (c == '1') S.mode = 1;
        if (c == '2') S.mode = 2;
        if (c == 27) {
            pthread_mutex_lock(&S.lock);
            S.quit = true;
            pthread_mutex_unlock(&S.lock);
            break;
        }
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