CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -pthread -lrt

SRCS_COMMON = world.c walker.c simulation.c

SERVER_SRCS = main_server.c server.c ipc.c $(SRCS_COMMON)
CLIENT_SRCS = main_client.c client.c ipc.c

TARGET_SERVER = server
TARGET_CLIENT = client

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_SERVER): $(SERVER_SRCS)
	$(CC) $(CFLAGS) $(SERVER_SRCS) -o $(TARGET_SERVER)

$(TARGET_CLIENT): $(CLIENT_SRCS)
	$(CC) $(CFLAGS) $(CLIENT_SRCS) -o $(TARGET_CLIENT)

clean:
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT)