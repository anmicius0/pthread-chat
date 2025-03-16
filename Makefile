# Define compiler and flags for building the project
CC = gcc
CFLAGS = -Wall -pthread

# Build both server and client programs
all: server client

# Compile the server
server: server.c common.h
	$(CC) $(CFLAGS) -o server server.c

# Compile the client
client: client.c common.h
	$(CC) $(CFLAGS) -o client client.c

# Clean up compiled executables
clean:
	rm -f server client
