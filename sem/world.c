#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "simulation.h"
#include "world.h"

#define SAVED_DIR "saved"

// Svet simulácie: alokácia matíc, načítanie prekážok a ukladanie výsledkov.

// Alokuje 2D polia pre štatistiky a prekážky podľa world_size.
void allocate_world(SharedState *S)
{
    S->total_steps = malloc(S->world_size * sizeof(int*));
    S->success_count = malloc(S->world_size * sizeof(int*));
    S->obstacles = malloc(S->world_size * sizeof(int*));

    for (int i = 0; i < S->world_size; i++) {
        S->total_steps[i] = calloc(S->world_size, sizeof(int));
        S->success_count[i] = calloc(S->world_size, sizeof(int));
        S->obstacles[i] = calloc(S->world_size, sizeof(int));
    }
}

// Uvoľní všetky dynamicky alokované matice sveta.
void free_world(SharedState *S)
{
    for (int i = 0; i < S->world_size; i++) {
        free(S->total_steps[i]);
        free(S->success_count[i]);
        free(S->obstacles[i]);
    }
    free(S->total_steps);
    free(S->success_count);
    free(S->obstacles);
}

// Vyplní polia nulami (čistý svet bez prekážok).
void initialize_world(SharedState *S)
{
    for (int i = 0; i < S->world_size; i++)
        for (int j = 0; j < S->world_size; j++) {
            S->total_steps[i][j] = 0;
            S->success_count[i][j] = 0;
            S->obstacles[i][j] = 0;
        }
}

// Zistí veľkosť sveta zo súboru s prekážkami (prvé číslo v súbore).
int get_world_size_from_obstacles(const char* filename)
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        return -1;
    }

    int size = -1;
    if (fscanf(file, "%d", &size) != 1 || size <= 0) {
        fclose(file);
        return -1;
    }

    fclose(file);
    return size;
}

// Načíta maticu prekážok zo súboru a validuje veľkosť.
int load_obstacles(SharedState *S, const char* filename)
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open obstacles file '%s'.\n", filename);
        return 0; // File not found
    }

    int size = 0;
    if (fscanf(file, "%d", &size) != 1 || size <= 0) {
        printf("Error: Invalid or missing size in obstacles file '%s'.\n", filename);
        fclose(file);
        return 0;
    }

    if (size != S->world_size) {
        printf("Error: Obstacles file size %d does not match configured world size %d.\n", size, S->world_size);
        fclose(file);
        return 0;
    }

    // Read obstacles matrix
    for (int i = 0; i < S->world_size; i++) {
        for (int j = 0; j < S->world_size; j++) {
            if (fscanf(file, "%d", &S->obstacles[i][j]) != 1) {
                printf("Error: Invalid obstacles file format at position [%d][%d].\n", i, j);
                fclose(file);
                initialize_world(S); // Reset obstacles
                return 0;
            }
        }
    }

    // Ensure center is not blocked (where walker starts)
    int center = S->world_size / 2;
    if (S->obstacles[center][center] == 1) {
        printf("Warning: Obstacle at center position removed.\n");
        S->obstacles[center][center] = 0;
    }

    fclose(file);
    printf("Obstacles loaded successfully from '%s'.\n", filename);
    return 1; // Success
}

// Uloží celý stav simulácie (konfiguráciu, prekážky, štatistiky) do súboru.
int save_simulation_results(SharedState *S, const char* filename)
{
    if (!S || !filename || filename[0] == '\0') {
        printf("Error: Invalid parameters for saving simulation.\n");
        return 0;
    }

    // Vytvor priečinok saved/ ak neexistuje
    struct stat st = {0};
    if (stat(SAVED_DIR, &st) == -1) {
        mkdir(SAVED_DIR, 0755);
    }

    // Vytvor celú cestu k súboru
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", SAVED_DIR, filename);

    FILE *f = fopen(filepath, "w");
    if (!f) {
        printf("Error: Could not open file '%s' for writing.\n", filepath);
        return 0;
    }

    // 1. Konfigurácia
    fprintf(f, "%d\n", S->world_size);
    fprintf(f, "%d\n", S->replications);
    fprintf(f, "%d\n", S->max_steps);
    fprintf(f, "%.6f %.6f %.6f %.6f\n", 
            S->prob.up, S->prob.down, S->prob.left, S->prob.right);
    fprintf(f, "%d\n", S->use_obstacles ? 1 : 0);

    // 2. Obstacles matrix
    for (int y = 0; y < S->world_size; y++) {
        for (int x = 0; x < S->world_size; x++) {
            fprintf(f, "%d ", S->obstacles[y][x]);
        }
        fprintf(f, "\n");
    }

    // 3. Statistics (total_steps a success_count)
    for (int y = 0; y < S->world_size; y++) {
        for (int x = 0; x < S->world_size; x++) {
            fprintf(f, "%d %d ", S->total_steps[y][x], S->success_count[y][x]);
        }
        fprintf(f, "\n");
    }

    fclose(f);
    printf("[Server] Results saved to '%s'\n", filepath);
    return 1;
}

// Obnoví predchádzajúcu simuláciu zo súboru.
int load_previous_simulation(SharedState *S, const char* filename)
{
    if (!S || !filename) {
        printf("Error: Invalid parameters for loading simulation.\n");
        return 0;
    }

    // Vytvor celú cestu k súboru
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", SAVED_DIR, filename);

    FILE *f = fopen(filepath, "r");
    if (!f) {
        printf("Error: Could not open file '%s' for reading.\n", filepath);
        return 0;
    }

    int world_size, replications, max_steps, use_obstacles;
    double prob_up, prob_down, prob_left, prob_right;

    // Načítaj konfiguráciu
    if (fscanf(f, "%d", &world_size) != 1 ||
        fscanf(f, "%d", &replications) != 1 ||
        fscanf(f, "%d", &max_steps) != 1 ||
        fscanf(f, "%lf %lf %lf %lf", &prob_up, &prob_down, &prob_left, &prob_right) != 4 ||
        fscanf(f, "%d", &use_obstacles) != 1) {
        printf("Error: Invalid file format (configuration section).\n");
        fclose(f);
        return 0;
    }

    // Nastav SharedState
    S->world_size = world_size;
    S->replications = replications;
    S->max_steps = max_steps;
    S->prob.up = prob_up;
    S->prob.down = prob_down;
    S->prob.left = prob_left;
    S->prob.right = prob_right;
    S->use_obstacles = (use_obstacles == 1);

    // Alokuj pamäť pre mapy
    allocate_world(S);

    // Načítaj obstacles matrix
    for (int y = 0; y < world_size; y++) {
        for (int x = 0; x < world_size; x++) {
            if (fscanf(f, "%d", &S->obstacles[y][x]) != 1) {
                printf("Error: Failed to load obstacles at [%d][%d].\n", y, x);
                fclose(f);
                free_world(S);
                return 0;
            }
        }
    }

    // Načítaj statistics (total_steps a success_count)
    for (int y = 0; y < world_size; y++) {
        for (int x = 0; x < world_size; x++) {
            if (fscanf(f, "%d %d", &S->total_steps[y][x], &S->success_count[y][x]) != 2) {
                printf("Error: Failed to load statistics at [%d][%d].\n", y, x);
                fclose(f);
                free_world(S);
                return 0;
            }
        }
    }

    fclose(f);
    printf("[Server] Simulation loaded from '%s'\n", filepath);
    printf("  World: %dx%d, Replications: %d, Max steps: %d\n", 
           world_size, world_size, replications, max_steps);
    return 1;
}