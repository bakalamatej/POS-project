#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>

#include "client.h"
#include "ipc.h"
#include "world.h"

static const char *SHM_NAME = "/pos_shm";
static const char *SOCK_PATH = "/tmp/pos_socket";

static volatile sig_atomic_t stop_flag = 0;
static struct termios orig_termios;
static bool termios_saved = false;

static void handle_sigint(int sig)
{
    (void)sig;
    stop_flag = 1;
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

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

typedef struct ClientCtx {
    int sock_fd;
    IPCShared *ipc;
    int summary_view;          // Lokálny summary_view pre tohto klienta
    pthread_mutex_t view_lock; // Lock pre summary_view
} ClientCtx;

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

        CLEAR_SCREEN();
        int n = ipc->world_size;
        if (n <= 0) n = 1;
        if (n > IPC_MAX_WORLD) n = IPC_MAX_WORLD;

        // Získaj lokálny summary_view
        pthread_mutex_lock(&ctx->view_lock);
        int local_view = ctx->summary_view;
        pthread_mutex_unlock(&ctx->view_lock);

        printf("=== RANDOM WALKER (CLIENT) ===\n");
        printf("Mode: %s | Summary view: %s\n",
               (ipc->mode == 1) ? "interactive" : "summary",
               (local_view == 0) ? "average steps" : "probability");
        printf("Replication %d / %d\n", ipc->current_rep, ipc->replications);
        printf("Finished: %s | Quit: %s\n",
               ipc->finished ? "yes" : "no",
               ipc->quit ? "yes" : "no");

        if (ipc->mode == 1) {
            printf("\nInteractive view (W=walker, *=center, #=obstacle)\n");
            for (int y = 0; y < n; y++) {
                for (int x = 0; x < n; x++) {
                    if (ipc->obstacles[y][x]) {
                        printf("# ");
                    } else if (y == ipc->walker_y && x == ipc->walker_x) {
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
                        if (ipc->obstacles[y][x]) {
                            printf(" ###");
                        } else if (ipc->success_count[y][x] > 0) {
                            int avg = ipc->total_steps[y][x] / ipc->success_count[y][x];
                            printf("%4d", avg);
                        } else {
                            printf("  --");
                        }
                    }
                    printf("\n");
                }
            } else {
                printf("\nProbability of reaching center (%%):\n");
                int denom = ipc->replications > 0 ? ipc->replications : 1;
                for (int y = 0; y < n; y++) {
                    for (int x = 0; x < n; x++) {
                        if (ipc->obstacles[y][x]) {
                            printf(" ###");
                        } else {
                            int prob = (ipc->success_count[y][x] * 100) / denom;
                            printf("%4d", prob);
                        }
                    }
                    printf("\n");
                }
            }
        }

        if (ipc->world_size > IPC_MAX_WORLD) {
            printf("\nPoznámka: zobrazuje sa len prvých %d x %d buniek (IPC limit).\n", IPC_MAX_WORLD, IPC_MAX_WORLD);
        }

        printf("\nOvládanie: 1=interaktívny, 2=sumárny, p=prepni summary view, q=odpojiť\n");

        // Automatické odpojenie po dokončení simulácie
        if (ipc->finished) {
            printf("\n[Klient] Simulácia dokončená. Odpájam sa...\n");
            sleep(2);
            stop_flag = 1;
            break;
        }

        struct timespec ts = {0, 300 * 1000000L};
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
        } else if (ch == 'p' || ch == 'P') {
            // Toggle lokálny summary view (len pre tohto klienta)
            pthread_mutex_lock(&ctx->view_lock);
            ctx->summary_view = 1 - ctx->summary_view;
            pthread_mutex_unlock(&ctx->view_lock);
        } else if (ch == 'q' || ch == 'Q') {
            printf("\n[Klient] Odpájam sa od servera...\n");
            stop_flag = 1;
            break;
        }
    }
    return NULL;
}

static int start_server_process(void)
{
    // Spustí server na pozadí; predpokladá, že binárka sa volá "server" v aktuálnom adresári.
    int rc = system("./server >/dev/null 2>&1 &");
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

        // Vyčisti obrazovku pred zobrazením menu
        CLEAR_SCREEN();
        
        printf("\n==============================\n");
        printf("   RANDOM WALKER - KLIENT\n");
        printf("==============================\n");
        printf("[1] Nová simulácia (spustí server)\n");
        printf("[2] Pripojiť sa k existujúcej simulácii\n");
        printf("[3] Koniec\n");

        // Čítaj znak priamo (BEZ Enter)
        char choice = getchar();

        if (choice == '3') {
            disable_raw_mode();
            printf("\nUkončujem klienta.\n");
            return 0;
        }

        if (choice != '1' && choice != '2') {
            printf("\nNeplatná voľba.\n");
            sleep(1);
            continue;
        }

        if (choice == '1') {
            printf("\nSpúšťam server...\n");
            if (start_server_process() != 0) {
                printf("Nepodarilo sa spustiť server.\n");
                sleep(2);
                continue;
            }
            printf("[Klient] Server spustený na pozadí.\n");
            sleep(1);
        }

        printf("[Klient] Pripájam sa k serveru...\n");
        int sock_fd = connect_with_retries(SOCK_PATH, 50, 100);
        if (sock_fd < 0) {
            printf("[Klient] Nepodarilo sa pripojiť k server socketu.\n");
            sleep(2);
            continue;
        }

        send_cmd(sock_fd, "PING\n");

        IPCShared *ipc = open_shm_with_retries(SHM_NAME, 50, 100);
        if (!ipc) {
            printf("[Klient] Nepodarilo sa otvoriť zdieľanú pamäť.\n");
            ipc_close_socket(sock_fd);
            sleep(2);
            continue;
        }

        printf("[Klient] Pripojený k serveru.\n");
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
        printf("[Klient] Odpojený od servera.\n");
        sleep(1);
        
        // Slučka pokračuje a zobrazí sa opäť menu
    }
    
    return 0;
}
