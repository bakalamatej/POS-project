#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "client.h"
#include "ipc.h"
#include "world.h"

// Konštanty pre timeouty a intervaly
#define RENDER_INTERVAL_MS 100
#define CONNECTION_RETRY_ATTEMPTS 10
#define CONNECTION_RETRY_SLEEP_MS 100
#define MAX_SERVERS 20
#define MAX_FILES 50
#define SAVED_DIR "saved"

// Názvy budú dynamicky načítané podľa výberu servera
// static const char *SHM_NAME = "/pos_shm";
// static const char *SOCK_PATH = "/tmp/pos_socket";

static volatile sig_atomic_t stop_flag = 0;
static struct termios orig_termios;
static bool termios_saved = false;

// Forward declaration
static void disable_raw_mode(void);

static void handle_sigint(int sig)
{
    (void)sig;
    stop_flag = 1;
    disable_raw_mode();  // Obnov terminál pri Ctrl+C
}

static void disable_raw_mode(void)
{
    if (termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        termios_saved = false;
    }
}

static void enable_raw_mode(void)
{
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        return;
    }
    termios_saved = true;
    atexit(disable_raw_mode);  // Zabezpeč cleanup pri exit()

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Vytlačí hlavičku aplikácie a vyčistí obrazovku
static void print_header(void)
{
    CLEAR_SCREEN();
    printf("\n==============================\n");
    printf("   RANDOM WALKER - CLIENT\n");
    printf("==============================\n");
}

typedef struct ServerInfo {
    int pid;
    char shm_name[64];
    char sock_path[128];
} ServerInfo;

static int list_available_servers(ServerInfo servers[], int max_servers)
{
    FILE *info = fopen("/tmp/pos_server_list.txt", "r");
    if (!info) return 0;
    
    int count = 0;
    while (count < max_servers && 
           fscanf(info, "PID=%d SHM=%63s SOCK=%127s\n", 
                  &servers[count].pid, 
                  servers[count].shm_name, 
                  servers[count].sock_path) == 3) {
        count++;
    }
    fclose(info);
    return count;
}

static int get_latest_server(char *shm_name, char *sock_path)
{
    ServerInfo servers[MAX_SERVERS];
    int count = list_available_servers(servers, MAX_SERVERS);
    
    if (count == 0) {
        return -1;
    }
    
    // Vráť posledný (najnovší) server zo zoznamu
    strcpy(shm_name, servers[count - 1].shm_name);
    strcpy(sock_path, servers[count - 1].sock_path);
    return 0;
}

static int select_server(char *shm_name, char *sock_path)
{
    ServerInfo servers[MAX_SERVERS];
    int count = list_available_servers(servers, MAX_SERVERS);
    
    if (count == 0) {
        printf("\n[Client] No active servers found.\n");
        printf("Please start a new simulation first.\n");
        sleep(2);
        return -1;
    }
    
    disable_raw_mode();
    
    print_header();
    printf("Available servers:\n\n");
    for (int i = 0; i < count; i++) {
        printf("  [%d] Server PID: %d\n", i + 1, servers[i].pid);
    }
    printf("\nSelect server [1-%d]: ", count);
    
    int choice;
    if (scanf("%d", &choice) != 1 || choice < 1 || choice > count) {
        while (getchar() != '\n');
        enable_raw_mode();
        return -1;
    }
    while (getchar() != '\n');
    
    enable_raw_mode();
    
    // Skopíruj vybraté info
    strcpy(shm_name, servers[choice - 1].shm_name);
    strcpy(sock_path, servers[choice - 1].sock_path);
    
    return 0;
}

static int list_result_files(char filenames[][256], int max_files)
{
    DIR *dir = opendir(SAVED_DIR);
    if (!dir) {
        // Ak priečinok neexistuje, vytvor ho
        mkdir(SAVED_DIR, 0755);
        dir = opendir(SAVED_DIR);
        if (!dir) {
            printf("Error: Could not open or create '%s' directory.\n", SAVED_DIR);
            return 0;
        }
    }
    
    int count = 0;
    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL && count < max_files) {
        // Filtruj len .txt súbory
        const char *name = entry->d_name;
        size_t len = strlen(name);
        
        if (len > 4 && strcmp(name + len - 4, ".txt") == 0) {
            strncpy(filenames[count], name, 255);
            filenames[count][255] = '\0';
            count++;
        }
    }
    
    closedir(dir);
    return count;
}

static int select_result_file(char *selected_file)
{
    char filenames[MAX_FILES][256];
    int count = list_result_files(filenames, MAX_FILES);
    
    if (count == 0) {
        printf("\n[Client] No .txt files found in '%s/' directory.\n", SAVED_DIR);
        printf("Please create a simulation first to generate result files.\n");
        sleep(2);
        return -1;
    }
    
    disable_raw_mode();
    
    print_header();
    printf("Available result files in '%s/':\n\n", SAVED_DIR);
    for (int i = 0; i < count; i++) {
        printf("  [%d] %s\n", i + 1, filenames[i]);
    }
    printf("\nSelect file [1-%d]: ", count);
    
    int choice;
    if (scanf("%d", &choice) != 1 || choice < 1 || choice > count) {
        printf("Invalid choice.\n");
        while (getchar() != '\n');
        enable_raw_mode();
        return -1;
    }
    while (getchar() != '\n');
    
    enable_raw_mode();
    
    // Skopíruj vybraný súbor
    strcpy(selected_file, filenames[choice - 1]);
    
    return 0;
}

typedef struct SimParams {
    int world_size;
    int replications;
    int max_steps;
    double prob_up;
    double prob_down;
    double prob_left;
    double prob_right;
    char obstacles_file[256];
    char output_file[256];
    int use_obstacles_file;
} SimParams;

typedef struct ClientCtx {
    int sock_fd;
    IPCShared *ipc;
    int summary_view;          // Lokálny summary_view pre tohto klienta
    pthread_mutex_t view_lock; // Lock pre summary_view
} ClientCtx;

static int get_simulation_params(SimParams *params)
{
    if (!params) return -1;
    
    // Dočasne vypni raw mode pre zadávanie parametrov
    disable_raw_mode();
    
    print_header();

    // World configuration
    printf("Use an obstacles file? \n[1] = yes\n[0] = no\n");
    if (scanf("%d", &params->use_obstacles_file) != 1) {
        enable_raw_mode();
        return -1;
    }
    
    if (params->use_obstacles_file) {
        strcpy(params->obstacles_file, "obstacles.txt");
        params->world_size = 0; // Veľkosť sa načíta zo súboru
    } else {    
        print_header();
        printf("World size (e.g. 10): \n");
        if (scanf("%d", &params->world_size) != 1 || params->world_size <= 0) {
            printf("Error: Invalid world dimensions.\n");
            enable_raw_mode();
            return -1;
        }
        params->obstacles_file[0] = '\0';
    }
    
    // Počet replikácií
    print_header();
    printf("Number of replications (e.g. 1000000): \n");
    if (scanf("%d", &params->replications) != 1 || params->replications <= 0) {
        printf("Error: Invalid number of replications.\n");
        enable_raw_mode();
        return -1;
    }
    
    // Maximálny počet krokov K
    print_header();
    printf("Maximum number of steps K (e.g. 100): \n");
    if (scanf("%d", &params->max_steps) != 1 || params->max_steps <= 0) {
        printf("Error: Invalid number of steps.\n");
        enable_raw_mode();
        return -1;
    }
    
    // Pravdepodobnosti
    print_header();
    printf("Do you want to enter movement probabilities? \n[1] = yes\n[0] = no (defaults to 0.25 each)\n");
    int prob_choice;
    if (scanf("%d", &prob_choice) == 1 && prob_choice == 1) {
        print_header();
        printf("Movement probabilities (4 numbers, sum = 1.0):\n");
        printf("  Hore: ");
        if (scanf("%lf", &params->prob_up) != 1) {
            enable_raw_mode();
            return -1;
        }
        printf("  Dole: ");
        if (scanf("%lf", &params->prob_down) != 1) {
            enable_raw_mode();
            return -1;
        }
        printf("  Vľavo: ");
        if (scanf("%lf", &params->prob_left) != 1) {
            enable_raw_mode();
            return -1;
        }
    
        // Dopocitanie pravdepodonosti vpravo
        params->prob_right = 1.0 - (params->prob_up + params->prob_down + params->prob_left);
        printf("Calculated right probability: %.2f\n", params->prob_right);
    
    } else {
        params->prob_up = 0.25;
        params->prob_down = 0.25;
        params->prob_left = 0.25;
        params->prob_right = 0.25;
    }
   
    
    // Výstupný súbor
    print_header();
    printf("Output file name (e.g. results.txt): ");
    if (scanf("%255s", params->output_file) != 1) {
        enable_raw_mode();
        return -1;
    }
    
    // Vypni buffer
    while (getchar() != '\n');
    
    // Zapni raw mode späť
    enable_raw_mode();
    
    return 0;
}

static void send_cmd(int fd, const char *cmd)
{
    if (fd < 0 || !cmd) return;
    write(fd, cmd, strlen(cmd));
}

static void *render_thread(void *arg)
{
    ClientCtx *ctx = (ClientCtx *)arg;
    while (!stop_flag) {
        IPCShared *ipc = ctx->ipc;
        if (!ipc) break;

        // Lokálne kopírovanie IPC dát (fix race condition)
        int world_size = ipc->world_size;
        int walker_x = ipc->walker_x;
        int walker_y = ipc->walker_y;
        int mode = ipc->mode;
        int current_rep = ipc->current_rep;
        int replications = ipc->replications;
        int finished = ipc->finished;
        
        // Skopíruj obstacles a štatistiky
        int n = world_size;
        if (n <= 0) n = 1;
        if (n > IPC_MAX_WORLD) n = IPC_MAX_WORLD;
        
        int local_obstacles[IPC_MAX_WORLD][IPC_MAX_WORLD];
        int local_total_steps[IPC_MAX_WORLD][IPC_MAX_WORLD];
        int local_success_count[IPC_MAX_WORLD][IPC_MAX_WORLD];
        
        for (int y = 0; y < n; y++) {
            for (int x = 0; x < n; x++) {
                local_obstacles[y][x] = ipc->obstacles[y][x];
                local_total_steps[y][x] = ipc->total_steps[y][x];
                local_success_count[y][x] = ipc->success_count[y][x];
            }
        }

        CLEAR_SCREEN();

        // Získaj lokálny summary_view
        pthread_mutex_lock(&ctx->view_lock);
        int local_view = ctx->summary_view;
        pthread_mutex_unlock(&ctx->view_lock);

        printf("=== RANDOM WALKER (CLIENT) ===\n");
        printf("Mode: %s | Summary view: %s\n",
               (mode == 1) ? "interactive" : "summary",
               (local_view == 0) ? "average steps" : "probability");
        printf("Replication %d / %d\n", current_rep, replications);
        printf("Finished: %s\n", finished ? "yes" : "no");

        if (mode == 1) {
            printf("\nInteractive view (W=walker, *=center, #=obstacle)\n");
            for (int y = 0; y < n; y++) {
                for (int x = 0; x < n; x++) {
                    if (local_obstacles[y][x]) {
                        printf("# ");
                    } else if (y == walker_y && x == walker_x) {
                        printf("W ");
                    } else if (y == n/2 && x == n/2) {
                        printf("* ");
                    } else {
                        printf(". ");
                    }
                }
                printf("\n");
            }
        } else {
            if (local_view == 0) {
                printf("\nAverage steps to reach center:\n");
                for (int y = 0; y < n; y++) {
                    for (int x = 0; x < n; x++) {
                        if (local_obstacles[y][x]) {
                            printf(" ###");
                        } else if (local_success_count[y][x] > 0) {
                            int avg = local_total_steps[y][x] / local_success_count[y][x];
                            printf("%4d", avg);
                        } else {
                            printf("  --");
                        }
                    }
                    printf("\n");
                }
            } else {
                printf("\nProbability of reaching center (%%):\n");
                int denom = replications > 0 ? replications : 1;
                for (int y = 0; y < n; y++) {
                    for (int x = 0; x < n; x++) {
                        if (local_obstacles[y][x]) {
                            printf(" ###");
                        } else {
                            int prob = (local_success_count[y][x] * 100) / denom;
                            printf("%4d", prob);
                        }
                    }
                    printf("\n");
                }
            }
        }

        if (world_size > IPC_MAX_WORLD) {
            printf("\nNote: only the first %d x %d cells are shown (IPC limit).\n", IPC_MAX_WORLD, IPC_MAX_WORLD);
        }

        printf("\nOPTIONS: \n[1] interactive \n[2] summary \n[3] toggle summary view \n[ESC] disconnect\n");

        // Automatic disconnect after simulation completes
        if (finished) {
            printf("\n[Client] Simulation completed. Press [ESC] to disconnect.\n");
        }

        struct timespec ts = {0, RENDER_INTERVAL_MS * 1000000L};
        nanosleep(&ts, NULL);
    }
    return NULL;
}

static void *input_thread(void *arg)
{
    ClientCtx *ctx = (ClientCtx *)arg;
    
    while (!stop_flag) {
        char ch = getchar();
        
        if (ch == '1') {
            send_cmd(ctx->sock_fd, "MODE 1\n");
        } else if (ch == '2') {
            send_cmd(ctx->sock_fd, "MODE 2\n");
        } else if (ch == '3') {
            // Toggle lokálny summary view (len pre tohto klienta)
            pthread_mutex_lock(&ctx->view_lock);
            ctx->summary_view = 1 - ctx->summary_view;
            pthread_mutex_unlock(&ctx->view_lock);
        } else if (ch == 27) {
            printf("\n[Client] Disconnecting from server...\n");
            stop_flag = 1;
            break;
        }
    }
    return NULL;
}

static int start_server_process(const SimParams *params)
{
    if (!params) return -1;
    
    // Vytvor príkaz na spustenie servera
    char cmd[1024];
    int n = snprintf(cmd, sizeof(cmd), "./server -r %d -k %d -p %.3f %.3f %.3f %.3f -o %s",
                     params->replications,
                     params->max_steps,
                     params->prob_up,
                     params->prob_down,
                     params->prob_left,
                     params->prob_right,
                     params->output_file);
    
    if (params->use_obstacles_file) {
        n += snprintf(cmd + n, sizeof(cmd) - n, " -f %s", params->obstacles_file);
    } else {
        n += snprintf(cmd + n, sizeof(cmd) - n, " -s %d", params->world_size);
    }
    
    // Pridaj presmerovanie a background
    snprintf(cmd + n, sizeof(cmd) - n, " >/dev/null 2>&1 &");
    
    printf("Launching server with parameters...\n");
    int rc = system(cmd);
    return (rc == -1) ? -1 : 0;
}

static int connect_with_retries(const char *sock_path, int attempts, int sleep_ms)
{
    for (int i = 0; i < attempts; i++) {
        int fd = ipc_connect_socket(sock_path);
        if (fd >= 0) return fd;
        usleep(sleep_ms * 1000);
    }
    return -1;
}

static IPCShared* open_shm_with_retries(const char *name, int attempts, int sleep_ms)
{
    for (int i = 0; i < attempts; i++) {
        IPCShared *ipc = NULL;
        if (ipc_open_shared(name, &ipc, false) == 0) return ipc;
        usleep(sleep_ms * 1000);
    }
    return NULL;
}

int client_run(void)
{
    signal(SIGINT, handle_sigint);
    
    // Zapni raw mode hneď na začiatku
    enable_raw_mode();

    while (1) {  // Hlavná slučka pre opakované menu
        stop_flag = 0;  // Reset flag pre nové pripojenie

        // Vytlač hlavičku
        print_header();
        printf("[ 1 ] New simulation (starts server)\n");
        printf("[ 2 ] Connect to an existing simulation\n");
        printf("[ 3 ] Resume previous simulation\n");
        printf("[ESC] Exit\n");

        // Čítaj znak priamo (BEZ Enter)
        char choice = getchar();

        if (choice == 27) {
            disable_raw_mode();
            printf("\nExiting client.\n");
            return 0;
        }

        if (choice != '1' && choice != '2' && choice != '3') {
            printf("\nInvalid option.\n");
            sleep(1);
            continue;
        }

        // Získaj IPC názvy
        char shm_name[64];
        char sock_path[128];
        
        if (choice == '1') {
            SimParams params;
            if (get_simulation_params(&params) != 0) {
                printf("\nError while entering parameters. Press any key...\n");
                getchar();
                continue;
            }
            
            printf("\nCreating a new simulation...\n");
            if (start_server_process(&params) != 0) {
                printf("Error: Failed to start server.\n");
                sleep(2);
                continue;
            }
            printf("[Client] Server started in the background.\n");
            sleep(2);  // Počkaj na spustenie servera
            
            // Automaticky sa pripoj na novo vytvorený server (posledný v zozname)
            if (get_latest_server(shm_name, sock_path) != 0) {
                printf("[Client] Failed to get server info.\n");
                sleep(2);
                continue;
            }
        } else if (choice == '3') {
            // Načítanie predchádzajúcej simulácie
            char resume_file[256];
            if (select_result_file(resume_file) != 0) {
                printf("Error: Failed to select file.\n");
                sleep(2);
                continue;
            }
            
            disable_raw_mode();
            print_header();
            printf("Selected file: %s\n\n", resume_file);
            
            int additional_reps;
            printf("Number of additional replications: ");
            if (scanf("%d", &additional_reps) != 1 || additional_reps <= 0) {
                printf("Error: Invalid number of replications.\n");
                enable_raw_mode();
                sleep(2);
                continue;
            }
            
            char output_file[256];
            printf("Output file name: ");
            if (scanf("%255s", output_file) != 1) {
                printf("Error: Invalid filename.\n");
                enable_raw_mode();
                sleep(2);
                continue;
            }
            
            while (getchar() != '\n');
            enable_raw_mode();
            
            // Spusti server s parametrom na načítanie
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "./server -l %s -r %d -o %s >/dev/null 2>&1 &",
                     resume_file, additional_reps, output_file);
            
            printf("\nResuming simulation...\n");
            if (system(cmd) == -1) {
                printf("Error: Failed to start server.\n");
                sleep(2);
                continue;
            }
            printf("[Client] Server started in the background.\n");
            sleep(2);
            
            if (get_latest_server(shm_name, sock_path) != 0) {
                printf("[Client] Failed to get server info.\n");
                sleep(2);
                continue;
            }
        } else {
            // Vyber zo zoznamu existujúcich serverov
            if (select_server(shm_name, sock_path) != 0) {
                printf("[Client] Failed to select server.\n");
                sleep(2);
                continue;
            }
        }

        printf("[Client] Connecting to server...\n");
        int sock_fd = connect_with_retries(sock_path, 50, 100);
        if (sock_fd < 0) {
            printf("[Client] Failed to connect to the server socket.\n");
            sleep(2);
            continue;
        }

        send_cmd(sock_fd, "PING\n");

        IPCShared *ipc = open_shm_with_retries(shm_name, 50, 100);
        if (!ipc) {
            printf("[Client] Failed to open shared memory.\n");
            ipc_close_socket(sock_fd);
            sleep(2);
            continue;
        }

        printf("[Client] Connected to server.\n");
        sleep(1);

        ClientCtx ctx = {
            .sock_fd = sock_fd,
            .ipc = ipc,
            .summary_view = 0,
            .view_lock = PTHREAD_MUTEX_INITIALIZER
        };

        pthread_t t_render, t_input;
        pthread_create(&t_render, NULL, render_thread, &ctx);
        pthread_create(&t_input, NULL, input_thread, &ctx);

        pthread_join(t_input, NULL);
        stop_flag = 1;
        pthread_join(t_render, NULL);

        ipc_close_shared(ipc);
        ipc_close_socket(sock_fd);
        printf("[Client] Disconnected from server.\n");
        sleep(1);
        
        // Slučka pokračuje a zobrazí sa opäť menu
    }
    
    return 0;
}
