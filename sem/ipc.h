#ifndef IPC_H
#define IPC_H

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdbool.h>

#define IPC_MAX_WORLD 64

// Zdieľaná štruktúra prenosu stavu medzi serverom a klientom.
typedef struct IPCShared {
	int world_size;
	int walker_x;
	int walker_y;
	int mode;
	int current_rep;
	int replications;
	int summary_view; // 0 = average steps, 1 = probability
	int finished;
	int obstacles[IPC_MAX_WORLD][IPC_MAX_WORLD];
	int total_steps[IPC_MAX_WORLD][IPC_MAX_WORLD];
	int success_count[IPC_MAX_WORLD][IPC_MAX_WORLD];
} IPCShared;

// Zdieľaná pamäť
int ipc_create_shared(const char *name, IPCShared **out);
int ipc_open_shared(const char *name, IPCShared **out, bool writeable);
void ipc_close_shared(IPCShared *ptr);
int ipc_unlink_shared(const char *name);

// Sockety
int ipc_listen_socket(const char *path);
int ipc_accept_socket(int listen_fd);
int ipc_connect_socket(const char *path);
void ipc_close_socket(int fd);

#endif // IPC_H
