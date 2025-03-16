#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 10     // Maximum number of clients supported
#define MAX_USERNAME 20    // Maximum username length
#define MAX_MESSAGE 256    // Maximum length of a message
#define WHITEBOARD_SIZE 10 // Number of messages to store on the whiteboard

// Message types: defines various message actions
typedef enum
{
    MSG_LOGIN,
    MSG_LOGOUT,
    MSG_BROADCAST,
    MSG_PRIVATE,
    MSG_ERROR
} MessageType;

// Message structure for communication
typedef struct
{
    MessageType type;
    char sender[MAX_USERNAME];
    char recipient[MAX_USERNAME]; // Recipient for private messaging
    char content[MAX_MESSAGE];
} Message;

// ANSI color codes used for terminal output formatting
#define ANSI_RESET "\x1b[0m"
#define ANSI_BOLD "\x1b[1m"
#define ANSI_RED "\x1b[31m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_BLUE "\x1b[34m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_CYAN "\x1b[36m"
#define ANSI_WHITE "\x1b[37m"

#endif // COMMON_H
