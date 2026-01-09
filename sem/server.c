#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

#include "server.h"
#include "walker.h"
#include "world.h"
#include "ipc.h"

// Konštanty pre timeouty a intervaly
#define SELECT_TIMEOUT_MS 100
#define SOCKET_POLL_INTERVAL_MS 50
#define MAIN_LOOP_INTERVAL_MS 100
#define SERVER_SHUTDOWN_DELAY_MS 2000

// Názvy budú dynamicky generované podľa PID
// static const char *SHM_NAME = "/pos_shm";
// static const char *SOCK_PATH = "/tmp/pos_socket";

typedef struct ClientConn {
    int fd;
    SharedState *S;
} ClientConn;

typedef struct SocketThreadArgs {
    SharedState *S;
    char *sock_path;
} SocketThreadArgs;

static int clamp_world_size(const SharedState *S)
{
    if (!S) return 0;
    return (S->world_size < IPC_MAX_WORLD) ? S->world_size : IPC_MAX_WORLD;
}

// Zlúčená funkcia pre synchronizáciu všetkých dát do IPC
static void sync_full_state_to_ipc(SharedState *S)
{
    if (!S || !S->ipc) return;
    int n = clamp_world_size(S);
    
    // Základné informácie
    S->ipc->world_size = n;
    int wx = S->walker.x;
    int wy = S->walker.y;
    if (wx >= n) wx %= n;
    if (wy >= n) wy %= n;
    S->ipc->walker_x = wx;
    S->ipc->walker_y = wy;
    S->ipc->mode = S->mode;
    S->ipc->summary_view = S->summary_view;
    S->ipc->current_rep = S->current_rep;
    S->ipc->replications = S->replications;
    S->ipc->finished = S->finished ? 1 : 0;
    
    // Prekážky a štatistiky
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            S->ipc->obstacles[y][x] = S->obstacles[y][x];
            S->ipc->total_steps[y][x] = S->total_steps[y][x];
            S->ipc->success_count[y][x] = S->success_count[y][x];
        }
    }
}

static void send_str(int fd, const char *msg)
{
    if (!msg) return;
    write(fd, msg, strlen(msg));
}

static void *client_handler_thread(void *arg)
{
    ClientConn *ctx = (ClientConn *)arg;
    int fd = ctx->fd;
    SharedState *S = ctx->S;
    char buf[256];
    ssize_t nread;
    while (1) {
        pthread_mutex_lock(&S->lock);
        bool finished = S->finished;
        pthread_mutex_unlock(&S->lock);
        if (finished) break;

        nread = read(fd, buf, sizeof(buf)-1);
        if (nread <= 0) {
            break;
        }
        buf[nread] = '\0';

        // očakávame jednoduché textové príkazy
        if (strncmp(buf, "PING", 4) == 0) {
            send_str(fd, "PONG\n");
        } else if (strncmp(buf, "MODE 1", 6) == 0) {
            pthread_mutex_lock(&S->lock);
            S->mode = 1;
            sync_full_state_to_ipc(S);
            pthread_mutex_unlock(&S->lock);
            send_str(fd, "OK\n");
        } else if (strncmp(buf, "MODE 2", 6) == 0) {
            pthread_mutex_lock(&S->lock);
            S->mode = 2;
            sync_full_state_to_ipc(S);
            pthread_mutex_unlock(&S->lock);
            send_str(fd, "OK\n");
        } else if (strncmp(buf, "SUMMARY 0", 10) == 0) {
            pthread_mutex_lock(&S->lock);
            S->summary_view = 0;
            sync_full_state_to_ipc(S);
            pthread_mutex_unlock(&S->lock);
            send_str(fd, "OK\n");
        } else if (strncmp(buf, "SUMMARY 1", 10) == 0) {
            pthread_mutex_lock(&S->lock);
            S->summary_view = 1;
            sync_full_state_to_ipc(S);
            pthread_mutex_unlock(&S->lock);
            send_str(fd, "OK\n");
        } else {
            send_str(fd, "ERR\n");
        }
    }

    printf("[Server] Klient sa odpojil.\n");
    ipc_close_socket(fd);
    free(ctx);
    return NULL;
}

static void *socket_thread(void *arg)
{
    SocketThreadArgs *args = (SocketThreadArgs *)arg;
    SharedState *S = args->S;
    char *sock_path = args->sock_path;
    
    int listen_fd = ipc_listen_socket(sock_path);
    if (listen_fd < 0) {
        free(sock_path);
        free(args);
        return NULL;
    }

    int first_client = 1;
    while (1) {
        pthread_mutex_lock(&S->lock);
        bool finished = S->finished;
        pthread_mutex_unlock(&S->lock);
        if (finished) break;

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listen_fd, &read_fds);
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = SELECT_TIMEOUT_MS * 1000;
        int ret = select(listen_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (ret > 0 && FD_ISSET(listen_fd, &read_fds)) {
            int cfd = ipc_accept_socket(listen_fd);
            if (cfd >= 0) {
                if (first_client) {
                    pthread_mutex_lock(&S->lock);
                    S->client_connected = 1;
                    pthread_mutex_unlock(&S->lock);
                    first_client = 0;
                }
                ClientConn *ctx = malloc(sizeof(ClientConn));
                if (!ctx) {
                    const char msg[] = "ERR no mem\n";
                    write(cfd, msg, sizeof(msg) - 1);
                    ipc_close_socket(cfd);
                } else {
                    ctx->fd = cfd;
                    ctx->S = S;
                    pthread_t th;
                    pthread_create(&th, NULL, client_handler_thread, ctx);
                    pthread_detach(th);
                }
            }
        }
    }

    ipc_close_socket(listen_fd);
    unlink(sock_path);
    free(sock_path);
    free(args);
    return NULL;
}

int server_run(const ServerConfig *config)
{
    if (!config) return 1;
    
    srand(time(NULL));
    
    // Generuj unikátne názvy pre IPC na základe PID
    pid_t pid = getpid();
    char shm_name[64];
    char sock_path[128];
    snprintf(shm_name, sizeof(shm_name), "/pos_shm_%d", pid);
    snprintf(sock_path, sizeof(sock_path), "/tmp/pos_socket_%d", pid);
    
    // Zapíš info do súboru pre klientov
    FILE *info_file = fopen("/tmp/pos_server_list.txt", "a");
    if (info_file) {
        fprintf(info_file, "PID=%d SHM=%s SOCK=%s\n", pid, shm_name, sock_path);
        fclose(info_file);
    }

    SharedState S;
    memset(&S, 0, sizeof(S));

    // Použiť konfiguráciu z parametrov
    if (config->obstacles_file[0] != '\0') {
        int size_from_file = get_world_size_from_obstacles(config->obstacles_file);
        if (size_from_file > 0) {
            S.use_obstacles = true;
            S.world_size = size_from_file;
        } else {
            printf("Chyba: Nepodarilo sa načítať súbor s prekážkami '%s'.\n", config->obstacles_file);
            return 1;
        }
    } else {
        S.use_obstacles = false;
        S.world_size = config->world_size;
    }

    S.replications = config->replications;
    S.max_steps = config->max_steps;
    S.prob.up = config->prob_up;
    S.prob.down = config->prob_down;
    S.prob.left = config->prob_left;
    S.prob.right = config->prob_right;
    S.mode = 2;
    S.summary_view = 0;
    S.finished = false;
    S.client_connected = 0;

    printf("Starting simulation:\n");
    printf("  world_size = %d\n", S.world_size);
    printf("  replications = %d\n", S.replications);
    printf("  max_steps = %d\n", S.max_steps);
    printf("  probabilities = %.2f/%.2f/%.2f/%.2f\n", S.prob.up, S.prob.down, S.prob.left, S.prob.right);
    printf("  obstacles = %s\n", S.use_obstacles ? config->obstacles_file : "none");
    printf("  output = %s\n", config->output_file[0] ? config->output_file : "(none)");
    printf("  Server PID = %d\n", pid);
    printf("  SHM name = %s\n", shm_name);
    printf("  Socket path = %s\n", sock_path);

    IPCShared *ipc = NULL;
    if (ipc_create_shared(shm_name, &ipc) != 0) {
        printf("Chyba: nepodarilo sa vytvoriť zdieľanú pamäť.\n");
        return 1;
    }
    S.ipc = ipc;

    allocate_world(&S);
    initialize_world(&S);

    if (S.use_obstacles) {
        if (!load_obstacles(&S, config->obstacles_file)) {
            printf("Failed to load obstacles. Exiting.\n");
            free_world(&S);
            ipc_close_shared(ipc);
            ipc_unlink_shared(shm_name);
            return 1;
        }
    }

    S.current_rep = 0;
    walker_init(&S.walker, S.world_size/2, S.world_size/2);
    
    // Synchronizuj celý stav do IPC naraz
    sync_full_state_to_ipc(&S);

    pthread_mutex_init(&S.lock, NULL);

    // Priprav argumenty pre socket thread
    SocketThreadArgs *sock_args = malloc(sizeof(SocketThreadArgs));
    if (!sock_args) {
        printf("Chyba: nepodarilo sa alokovať pamäť pre socket thread.\n");
        free_world(&S);
        ipc_close_shared(ipc);
        ipc_unlink_shared(shm_name);
        return 1;
    }
    sock_args->S = &S;
    sock_args->sock_path = strdup(sock_path);


    pthread_t sim, walk, sock_thr;
    pthread_create(&sock_thr, NULL, socket_thread, sock_args);

    // Čakaj na pripojenie klienta
    printf("[Server] Waiting for client to connect before starting simulation...\n");
    while (1) {
        pthread_mutex_lock(&S.lock);
        int connected = S.client_connected;
        pthread_mutex_unlock(&S.lock);
        if (connected) break;
        usleep(100000);
    }
    printf("[Server] Client connected! Starting simulation...\n");

    pthread_create(&sim, NULL, simulation_thread, &S);
    pthread_create(&walk, NULL, walker_thread, &S);

    bool finished_noted = false;
    struct timespec finish_ts = {0};

    while (1) {
        pthread_mutex_lock(&S.lock);
        bool finished = S.finished;
        pthread_mutex_unlock(&S.lock);

        if (finished) {
            if (!finished_noted) {
                clock_gettime(CLOCK_MONOTONIC, &finish_ts);
                finished_noted = true;
            } else {
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                long elapsed_ms = (now.tv_sec - finish_ts.tv_sec) * 1000 +
                                  (now.tv_nsec - finish_ts.tv_nsec) / 1000000;
                if (elapsed_ms > SERVER_SHUTDOWN_DELAY_MS) {
                    break;
                }
            }
        }

        usleep(MAIN_LOOP_INTERVAL_MS * 1000);
    }

    pthread_mutex_lock(&S.lock);
    S.finished = true;
    sync_full_state_to_ipc(&S);
    pthread_mutex_unlock(&S.lock);

    pthread_join(sim, NULL);
    pthread_join(walk, NULL);
    pthread_join(sock_thr, NULL);

    pthread_mutex_destroy(&S.lock);

    free_world(&S);

    ipc_close_shared(ipc);
    ipc_unlink_shared(shm_name);
    
    // Odstráň tento server zo zoznamu
    FILE *list = fopen("/tmp/pos_server_list.txt", "r");
    FILE *temp = fopen("/tmp/pos_server_list_temp.txt", "w");
    if (list && temp) {
        char line[256];
        int line_pid;
        while (fgets(line, sizeof(line), list)) {
            if (sscanf(line, "PID=%d", &line_pid) == 1 && line_pid != pid) {
                fputs(line, temp);
            }
        }
        fclose(list);
        fclose(temp);
        remove("/tmp/pos_server_list.txt");
        rename("/tmp/pos_server_list_temp.txt", "/tmp/pos_server_list.txt");
    } else {
        if (list) fclose(list);
        if (temp) fclose(temp);
    }

    return 0;
}
