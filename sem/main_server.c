#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

static void print_usage(const char *prog)
{
    printf("Použitie: %s [OPTIONS]\n", prog);
    printf("Možnosti:\n");
    printf("  -s SIZE       Veľkosť sveta (napr. 10)\n");
    printf("  -r REPS       Počet replikácií (napr. 1000000)\n");
    printf("  -k STEPS      Maximálny počet krokov K (napr. 100)\n");
    printf("  -p U D L R    Pravdepodobnosti (hore dole vľavo vpravo)\n");
    printf("  -f FILE       Súbor s prekážkami\n");
    printf("  -o FILE       Výstupný súbor\n");
    printf("  -h            Zobraz túto pomoc\n");
}

int main(int argc, char *argv[])
{
    ServerConfig config;
    memset(&config, 0, sizeof(config));
    
    // Predvolené hodnoty
    config.world_size = 10;
    config.replications = 1000000;
    config.max_steps = 100;
    config.prob_up = 0.25;
    config.prob_down = 0.25;
    config.prob_left = 0.25;
    config.prob_right = 0.25;
    config.obstacles_file[0] = '\0';
    config.output_file[0] = '\0';
    
    int opt;
    while ((opt = getopt(argc, argv, "s:r:k:p:f:o:h")) != -1) {
        switch (opt) {
            case 's':
                config.world_size = atoi(optarg);
                break;
            case 'r':
                config.replications = atoi(optarg);
                break;
            case 'k':
                config.max_steps = atoi(optarg);
                break;
            case 'p':
                if (optind + 3 <= argc) {
                    config.prob_up = atof(optarg);
                    config.prob_down = atof(argv[optind++]);
                    config.prob_left = atof(argv[optind++]);
                    config.prob_right = atof(argv[optind++]);
                }
                break;
            case 'f':
                strncpy(config.obstacles_file, optarg, sizeof(config.obstacles_file) - 1);
                break;
            case 'o':
                strncpy(config.output_file, optarg, sizeof(config.output_file) - 1);
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    return server_run(&config);
}
