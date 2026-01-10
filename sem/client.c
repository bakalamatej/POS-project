#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
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
#include "utils.h"

// Klientská aplikácia: výber servera, načítanie konfigurácie a terminálové zobrazenie simulácie.
#define RENDER_INTERVAL_MS 100
#define MAX_SERVERS 20
#define MAX_FILES 50
#define SAVED_DIR "saved"
#define CONNECT_RETRIES 50
#define CONNECT_SLEEP_MS 100

static volatile sig_atomic_t stop_flag = 0;
static struct termios orig_termios;

static void handle_sigint(int sig) { (void)sig; stop_flag = 1; }

typedef struct {
    int world_size, replications, max_steps;
    double prob_up, prob_down, prob_left, prob_right;
    char obstacles_file[256], output_file[256];
    int use_obstacles_file;
} SimParams;

typedef struct {
    int pid;
    char shm_name[64];
    char sock_path[128];
} ServerInfo;

typedef struct {
    int sock_fd;
    IPCShared *ipc;
    int summary_view;
    pthread_mutex_t view_lock;
    int server_pid;
} ClientCtx;

// ============ IPC HELPERS ============

// Spočíta dostupné servery zo súboru so zoznamom.
static int count_servers(ServerInfo *servers)
{
    FILE *f = fopen("/tmp/pos_server_list.txt", "r");
    if (!f) return 0;
    
    int count = 0;
    while (count < MAX_SERVERS &&
           fscanf(f, "PID=%d SHM=%63s SOCK=%127s\n",
                  &servers[count].pid, servers[count].shm_name,
                  servers[count].sock_path) == 3) {
        count++;
    }
    fclose(f);
    return count;
}

// Vyberie posledný (najnovší) server zo zoznamu.
static int get_latest_server(char *shm_name, char *sock_path)
{
    ServerInfo servers[MAX_SERVERS];
    int count = count_servers(servers);
    
    if (count == 0) return -1;
    safe_strcpy(shm_name, servers[count - 1].shm_name, 64);
    safe_strcpy(sock_path, servers[count - 1].sock_path, 128);
    return 0;
}

// Interaktívne nechá používateľa zvoliť server na pripojenie.
static int select_server(char *shm_name, char *sock_path)
{
    ServerInfo servers[MAX_SERVERS];
    int count = count_servers(servers);
    
    if (count == 0) {
        printf("[Client] No active servers.\n");
        return -1;
    }
    
    disable_raw_mode(&orig_termios);
    CLEAR_SCREEN();
    printf("Available servers:\n\n");
    for (int i = 0; i < count; i++)
        printf("  [%d] PID: %d\n", i + 1, servers[i].pid);
    printf("\nSelect [1-%d]: ", count);
    
    int choice;
    int ok = (scanf("%d", &choice) == 1 && choice >= 1 && choice <= count);
    while (getchar() != '\n');
    
    if (ok) {
        safe_strcpy(shm_name, servers[choice - 1].shm_name, 64);
        safe_strcpy(sock_path, servers[choice - 1].sock_path, 128);
    }
    enable_raw_mode(&orig_termios);
    return ok ? 0 : -1;
}

// ============ FILE HELPERS ============

// Vypíše uložené výsledky v priečinku saved/ a naplní zoznam názvov.
static int list_files(char filenames[][256])
{
    DIR *dir = opendir(SAVED_DIR);
    if (!dir) {
        mkdir(SAVED_DIR, 0755);
        dir = opendir(SAVED_DIR);
        if (!dir) return 0;
    }
    
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) && count < MAX_FILES) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len > 4 && !strcmp(name + len - 4, ".txt")) {
            safe_strcpy(filenames[count++], name, 256);
        }
    }
    closedir(dir);
    return count;
}

// Umožní vybrať súbor s uloženou simuláciou.
static int select_file(char *selected_file)
{
    char filenames[MAX_FILES][256];
    int count = list_files(filenames);
    
    if (count == 0) {
        printf("[Client] No saved files.\n");
        return -1;
    }
    
    disable_raw_mode(&orig_termios);
    CLEAR_SCREEN();
    printf("Saved simulations:\n\n");
    for (int i = 0; i < count; i++)
        printf("  [%d] %s\n", i + 1, filenames[i]);
    printf("\nSelect [1-%d]: ", count);
    
    int choice;
    int ok = (scanf("%d", &choice) == 1 && choice >= 1 && choice <= count);
    while (getchar() != '\n');
    
    if (ok) {
        safe_strcpy(selected_file, filenames[choice - 1], 256);
    }
    enable_raw_mode(&orig_termios);
    return ok ? 0 : -1;
}

// ============ INPUT HELPERS ============

// Vykreslí hlavičku interaktívneho menu klienta.
void print_header()
{
    CLEAR_SCREEN();
    printf("==============================\n"
           "=== RANDOM WALKER - CLIENT ===\n"
           "==============================\n\n");
}

// Bezpečne načíta celé číslo z konzoly.
static int read_int(const char *prompt, int *value)
{
    printf("%s", prompt);
    int ret = scanf("%d", value);
    while (getchar() != '\n');
    return (ret == 1) ? 0 : -1;
}

// Načíta tri desatinné čísla (pravdepodobnosti).
static int read_doubles(const char *prompt, double *a, double *b, double *c)
{
    printf("%s", prompt);
    int ret = scanf("%lf %lf %lf", a, b, c);
    while (getchar() != '\n');
    return (ret == 3) ? 0 : -1;
}

// Načíta reťazec (napr. názov súboru) z konzoly.
static int read_string(const char *prompt, char *buf, size_t size)
{
    printf("%s", prompt);
    char fmt[16];
    snprintf(fmt, sizeof(fmt), "%%%zus", size - 1);
    int ret = scanf(fmt, buf);
    while (getchar() != '\n');
    return (ret == 1) ? 0 : -1;
}

// Načíta reťazec (názov súboru) z konzoly.
static int get_params(SimParams *p)
{
    if (!p) return -1;
    
    disable_raw_mode(&orig_termios);
    print_header();

    if (read_int("Use obstacles? [1/0]: ", &p->use_obstacles_file) != 0) goto err;
    
    if (!p->use_obstacles_file) {
        if (read_int("World size: ", &p->world_size) != 0 || p->world_size <= 0) goto err;
    } else {
        safe_strcpy(p->obstacles_file, "obstacles.txt", 256);
    }
    
    if (read_int("Replications: ", &p->replications) != 0 || p->replications <= 0) goto err;
    if (read_int("Max steps: ", &p->max_steps) != 0 || p->max_steps <= 0) goto err;
    
    int custom;
    if (read_int("Custom probabilities? [1/0]: ", &custom) != 0) goto err;
    if (custom == 1) {
        if (read_doubles("Up Down Left: ", &p->prob_up, &p->prob_down, &p->prob_left) != 0) goto err;
        p->prob_right = 1.0 - (p->prob_up + p->prob_down + p->prob_left);
    } else {
        p->prob_up = p->prob_down = p->prob_left = p->prob_right = 0.25;
    }
    
    if (read_string("Output file: ", p->output_file, 256) != 0) goto err;
    
    enable_raw_mode(&orig_termios);
    return 0;

err:
    enable_raw_mode(&orig_termios);
    return -1;
}

// ============ CONNECTION ============

static void send_cmd(int fd, const char *cmd)
{
    if (fd >= 0 && cmd) write(fd, cmd, strlen(cmd));
}

// Odošle textový príkaz na socket servera.
static int connect_retry(const char *path)
{
    for (int i = 0; i < CONNECT_RETRIES; i++) {
        int fd = ipc_connect_socket(path);
        if (fd >= 0) return fd;
        usleep(CONNECT_SLEEP_MS * 1000);
    }
    return -1;
}

// Opakovane sa pokúsi pripojiť k UNIX socketu servera.
static IPCShared* open_shm_retry(const char *name)
{
    IPCShared *ipc = NULL;
    for (int i = 0; i < CONNECT_RETRIES; i++) {
        if (ipc_open_shared(name, &ipc, false) == 0) return ipc;
        usleep(CONNECT_SLEEP_MS * 1000);
    }
    return NULL;
}

// ============ THREADS ============

// Vlákno na zobrazovanie stavu simulácie v termináli.
static void *render_thread(void *arg)
{
    ClientCtx *ctx = (ClientCtx *)arg;
    int last_mode = -1, last_view = -1;
    
    while (!stop_flag) {
        IPCShared *ipc = ctx->ipc;
        if (!ipc) break;

        int n = ipc->world_size;
        if (n <= 0 || n > IPC_MAX_WORLD) n = IPC_MAX_WORLD;

        pthread_mutex_lock(&ctx->view_lock);
        int local_view = ctx->summary_view;
        pthread_mutex_unlock(&ctx->view_lock);

        if (last_mode != ipc->mode || last_view != local_view) {
            CLEAR_SCREEN();
            last_mode = ipc->mode;
            last_view = local_view;
        } else {
            MOVE_CURSOR();
        }

        print_header();

        if (ctx->server_pid > 0)
            printf("Server PID: %d\n", ctx->server_pid);
        printf("Mode: %s | View: %s | Rep %d/%d | Done: %s\n",
               ipc->mode == 1 ? "interactive" : "summary",
               local_view == 0 ? "avg" : "prob",
               ipc->current_rep, ipc->replications,
               ipc->finished ? "yes" : "no");

        if (ipc->mode == 1) {
            printf("\n(W=walker, *=center, #=obstacle)\n");
            for (int y = 0; y < n; y++) {
                for (int x = 0; x < n; x++) {
                    if (ipc->obstacles[y][x]) printf("# ");
                    else if (y == ipc->walker_y && x == ipc->walker_x) printf("W ");
                    else if (y == n/2 && x == n/2) printf("* ");
                    else printf(". ");
                }
                printf("\n");
            }
        } else {
            printf("\n%s:\n", local_view == 0 ? "Avg" : "Prob(%)");
            for (int y = 0; y < n; y++) {
                for (int x = 0; x < n; x++) {
                    if (ipc->obstacles[y][x]) printf(" ###");
                    else if (ipc->success_count[y][x] > 0) {
                        if (local_view == 0)
                            printf("%4d", ipc->total_steps[y][x] / ipc->success_count[y][x]);
                        else
                            printf("%4d", (ipc->success_count[y][x] * 100) / ipc->replications);
                    } else {
                        printf("  --");
                    }
                }
                printf("\n");
            }
        }

        printf("\n[1]interactive [2]summary [3]view [ESC]exit\n");
        if (ipc->finished) printf("[DONE]\n");

        struct timespec ts = {0, RENDER_INTERVAL_MS * 1000000L};
        nanosleep(&ts, NULL);
    }
    return NULL;
}

// Vlákno na spracovanie vstupu používateľa počas behu.
static void *input_thread(void *arg)
{
    ClientCtx *ctx = (ClientCtx *)arg;
    IPCShared *ipc = ctx->ipc;
    
    while (!stop_flag) {
        int ch = getchar();
        if (ipc && ipc->finished) {
            if (ch == 27) {
                stop_flag = 1;
                break;
            }
            continue;
        }
        if (ch == '1') send_cmd(ctx->sock_fd, "MODE 1\n");
        else if (ch == '2') send_cmd(ctx->sock_fd, "MODE 2\n");
        else if (ch == '3') {
            pthread_mutex_lock(&ctx->view_lock);
            ctx->summary_view = 1 - ctx->summary_view;
            pthread_mutex_unlock(&ctx->view_lock);
        } else if (ch == 27) {
            stop_flag = 1;
            break;
        }
    }
    return NULL;
}

// ============ MAIN ============

// Hlavná slučka klienta: výber servera, spustenie vlákien a čakanie na ukončenie.
int client_run(void)
{
    signal(SIGINT, handle_sigint);
    enable_raw_mode(&orig_termios);

    while (1) {
        stop_flag = 0;

        print_header();
        
        printf("[1] New simulation\n[2] Connect to server\n[3] Resume simulation\n\n[ESC] Exit\n");

        char choice = getchar();
        if (choice == 27) {
            disable_raw_mode(&orig_termios);
            printf("\nBye.\n");
            return 0;
        }

        char shm_name[64] = {0}, sock_path[128] = {0};
        int sock_fd = -1;
        IPCShared *ipc = NULL;

        if (choice == '1') {
            SimParams p = {0};
            if (get_params(&p) != 0) continue;
            
            char cmd[1024];
            int n = snprintf(cmd, 1024, "./server -r %d -k %d -p %.2f %.2f %.2f %.2f -o %s",
                p.replications, p.max_steps, p.prob_up, p.prob_down, p.prob_left, p.prob_right, p.output_file);
            if (p.use_obstacles_file)
                n += snprintf(cmd + n, 1024 - n, " -f %s", p.obstacles_file);
            else
                n += snprintf(cmd + n, 1024 - n, " -s %d", p.world_size);
            snprintf(cmd + n, 1024 - n, " >/dev/null 2>&1 &");
            
            if (system(cmd) == -1) continue;
            sleep(1);
            if (get_latest_server(shm_name, sock_path) != 0) continue;
        } else if (choice == '2') {
            if (select_server(shm_name, sock_path) != 0) continue;
        } else if (choice == '3') {
            char file[256];
            if (select_file(file) != 0) continue;
            
            disable_raw_mode(&orig_termios);
            int reps;
            char outf[256];
            printf("Reps: ");
            if (scanf("%d", &reps) != 1 || reps <= 0) {
                while (getchar() != '\n');
                enable_raw_mode(&orig_termios);
                continue;
            }
            printf("Output: ");
            if (scanf("%255s", outf) != 1) {
                while (getchar() != '\n');
                enable_raw_mode(&orig_termios);
                continue;
            }
            while (getchar() != '\n');
            
            char cmd[1024];
            snprintf(cmd, 1024, "./server -l %s -r %d -o %s >/dev/null 2>&1 &", file, reps, outf);
            if (system(cmd) == -1) {
                enable_raw_mode(&orig_termios);
                continue;
            }
            sleep(1);
            enable_raw_mode(&orig_termios);
            if (get_latest_server(shm_name, sock_path) != 0) continue;
        } else {
            printf("Wrong input.\n");
            sleep(1);
            continue;
        }

        sock_fd = connect_retry(sock_path);
        if (sock_fd < 0) {
            printf("Connect fail.\n");
            sleep(2);
            continue;
        }

        ipc = open_shm_retry(shm_name);
        if (!ipc) {
            printf("SHM fail.\n");
            ipc_close_socket(sock_fd);
            sleep(2);
            continue;
        }

        printf("Connected.\n");
        sleep(1);

        int pid = 0;
        sscanf(sock_path, "/tmp/pos_socket_%d", &pid);

        ClientCtx ctx = {
            .sock_fd = sock_fd, .ipc = ipc, .summary_view = 0,
            .view_lock = PTHREAD_MUTEX_INITIALIZER, .server_pid = pid
        };

        pthread_t tr, ti;
        pthread_create(&tr, NULL, render_thread, &ctx);
        pthread_create(&ti, NULL, input_thread, &ctx);

        pthread_join(ti, NULL);
        stop_flag = 1;
        pthread_join(tr, NULL);

        ipc_close_shared(ipc);
        ipc_close_socket(sock_fd);
        printf("Disconnected.\n");
        sleep(1);
    }

    return 0;
}