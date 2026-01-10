#define _POSIX_C_SOURCE 200809L
#include "ipc.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

// Správa zdieľanej pamäte a UNIX socketov pre komunikáciu server-klient.
// Vytvorí zdieľanú pamäť a vynuluje ju.
int ipc_create_shared(const char *name, IPCShared **out)
{
	if (!name || !out) return -1;

	int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
	if (fd == -1) return -1;

	if (ftruncate(fd, sizeof(IPCShared)) == -1) {
		close(fd);
		shm_unlink(name);
		return -1;
	}

	void *addr = mmap(NULL, sizeof(IPCShared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (addr == MAP_FAILED) {
		shm_unlink(name);
		return -1;
	}

	memset(addr, 0, sizeof(IPCShared));
	*out = (IPCShared *)addr;
	return 0;
}

// Otvorí existujúcu zdieľanú pamäť (len čítanie alebo aj zápis).
int ipc_open_shared(const char *name, IPCShared **out, bool writeable)
{
	if (!name || !out) return -1;
	int flags = writeable ? O_RDWR : O_RDONLY;
	int prot = writeable ? (PROT_READ | PROT_WRITE) : PROT_READ;

	int fd = shm_open(name, flags, 0666);
	if (fd == -1) return -1;

	void *addr = mmap(NULL, sizeof(IPCShared), prot, MAP_SHARED, fd, 0);
	close(fd);
	if (addr == MAP_FAILED) return -1;

	*out = (IPCShared *)addr;
	return 0;
}

// Odmapuje zdieľanú pamäť z procesu.
void ipc_close_shared(IPCShared *ptr)
{
	if (ptr) munmap(ptr, sizeof(IPCShared));
}

// Odstráni segment zdieľanej pamäte zo systému.
int ipc_unlink_shared(const char *name)
{
	if (!name) return -1;
	return shm_unlink(name);
}

// Vytvorí a začne počúvať na UNIX doménovom sockete.
int ipc_listen_socket(const char *path)
{
	if (!path) return -1;
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) return -1;

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	unlink(path); 

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		close(fd);
		return -1;
	}

	if (listen(fd, 8) == -1) {
		close(fd);
		return -1;
	}

	return fd;
}

// Prijme nové pripojenie na počúvajúcom sockete.
int ipc_accept_socket(int listen_fd)
{
	if (listen_fd < 0) return -1;
	return accept(listen_fd, NULL, NULL);
}

// Pripojí sa k UNIX doménovému socketu.
int ipc_connect_socket(const char *path)
{
	if (!path) return -1;
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) return -1;

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		close(fd);
		return -1;
	}

	return fd;
}

// Zavrie otvorený socket.
void ipc_close_socket(int fd)
{
	if (fd >= 0) close(fd);
}
