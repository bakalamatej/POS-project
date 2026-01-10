#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

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
    config.resume_file[0] = '\0';
    
    int opt;
    while ((opt = getopt(argc, argv, "s:r:k:p:f:l:o:h")) != -1) {
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
            case 'p': {
                config.prob_up = atof(optarg);
                int idx = optind;
                if (idx + 2 < argc) {
                    config.prob_down = atof(argv[idx++]);
                    config.prob_left = atof(argv[idx++]);
                    config.prob_right = atof(argv[idx++]);
                    optind = idx;
                }
                break;
            }
            case 'f':
                strncpy(config.obstacles_file, optarg, sizeof(config.obstacles_file) - 1);
                break;
            case 'l':
                strncpy(config.resume_file, optarg, sizeof(config.resume_file) - 1);
                break;
            case 'o':
                strncpy(config.output_file, optarg, sizeof(config.output_file) - 1);
                break;
        }
    }
    
    return server_run(&config);
}
