#include "common.h"
#include <time.h>
#include <stdarg.h>

void update_whiteboard(const char *message);

/**
 * Client structure - Represents a connected client
 * Contains socket descriptor and username
 */
typedef struct
{
    int socket;                  // Socket file descriptor for client connection
    char username[MAX_USERNAME]; // Client's username
} Client;

// Client list with mutex protection
Client clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int client_count = 0;

/**
 * Whiteboard structure - Implements a circular buffer for storing messages
 * to display in the server console
 */
typedef struct
{
    char messages[WHITEBOARD_SIZE][MAX_USERNAME + MAX_MESSAGE + 50]; // Array of messages
    int current_index;                                               // Current position in the circular buffer
} Whiteboard;

Whiteboard whiteboard = {{0}, 0};
pthread_mutex_t whiteboard_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Message type enumeration - Defines types of server messages
 */
typedef enum
{
    MSG_TYPE_BROADCAST,
    MSG_TYPE_PRIVATE,
    MSG_TYPE_LOGIN,
    MSG_TYPE_LOGOUT,
    MSG_TYPE_ERROR
} ServerMsgType;

/**
 * Returns the appropriate ANSI color and type string for a message type
 *
 * @param type The server message type
 * @param type_str Output parameter for the type string
 * @return The ANSI color code
 */
const char *get_msg_properties(ServerMsgType type, const char **type_str)
{
    switch (type)
    {
    case MSG_TYPE_BROADCAST:
        *type_str = "BROADCAST";
        return ANSI_BLUE;
    case MSG_TYPE_PRIVATE:
        *type_str = "PRIVATE";
        return ANSI_MAGENTA;
    case MSG_TYPE_LOGIN:
        *type_str = "LOGIN";
        return ANSI_GREEN;
    case MSG_TYPE_LOGOUT:
        *type_str = "LOGOUT";
        return ANSI_YELLOW;
    case MSG_TYPE_ERROR:
        *type_str = "ERROR";
        return ANSI_RED;
    default:
        *type_str = "INFO";
        return ANSI_WHITE;
    }
}

/**
 * Formats a message and adds it to the whiteboard
 *
 * @param type Message type identifier
 * @param format Printf-style format string
 * @param ... Variable arguments for formatting
 */
void format_whiteboard_msg(ServerMsgType type, const char *format, ...)
{
    char message[MAX_USERNAME + MAX_MESSAGE + 50];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    const char *type_str;
    const char *color = get_msg_properties(type, &type_str);

    // Create the full message with type prefix
    char full_message[MAX_USERNAME + MAX_MESSAGE + 50];
    snprintf(full_message, sizeof(full_message), "%s%s%s - %s",
             color, type_str, ANSI_RESET, message);

    // Add to whiteboard
    update_whiteboard(full_message);
}

/**
 * Adds a message to the whiteboard and refreshes the display
 *
 * Stores message in circular buffer and updates terminal display
 * with all messages and server status.
 *
 * @param message The message to add
 */
void update_whiteboard(const char *message)
{
    pthread_mutex_lock(&whiteboard_mutex);

    // Add message to circular buffer
    strncpy(whiteboard.messages[whiteboard.current_index], message,
            sizeof(whiteboard.messages[0]) - 1);
    whiteboard.current_index = (whiteboard.current_index + 1) % WHITEBOARD_SIZE;

    // Clear screen and show header
    printf("\033[2J\033[H");
    printf("%s╔══════════ SERVER WHITEBOARD ══════════╗%s\n", ANSI_BOLD, ANSI_RESET);

    // Display active client count (all clients in array are active)
    pthread_mutex_lock(&clients_mutex);
    int active = client_count;
    pthread_mutex_unlock(&clients_mutex);
    printf("Active clients: %s%d/%d%s\n\n", ANSI_GREEN, active, MAX_CLIENTS, ANSI_RESET);

    // Display messages
    for (int i = 0; i < WHITEBOARD_SIZE; i++)
    {
        int idx = (whiteboard.current_index + i) % WHITEBOARD_SIZE;
        if (whiteboard.messages[idx][0])
            printf("%s\n", whiteboard.messages[idx]);
    }

    printf("\n%s[Ctrl+C to exit]%s\n", ANSI_BOLD, ANSI_RESET);
    pthread_mutex_unlock(&whiteboard_mutex);
}

/**
 * Searches for a client by username
 *
 * @param username Username to search for
 * @param socket_out Optional pointer to store socket descriptor
 * @return 1 if client found, 0 otherwise
 */
int find_client(const char *username, int *socket_out)
{
    int found = 0;

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (strcmp(clients[i].username, username) == 0)
        {
            found = 1;
            if (socket_out)
                *socket_out = clients[i].socket;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    return found;
}

/**
 * Sends a message to all active clients except the sender
 *
 * @param msg Pointer to the Message structure
 * @param sender_socket Socket of sender to exclude
 */
void broadcast_message(Message *msg, int sender_socket)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].socket != sender_socket)
        {
            send(clients[i].socket, msg, sizeof(Message), 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    // Log the broadcast message
    format_whiteboard_msg(MSG_TYPE_BROADCAST, "%s: %s", msg->sender, msg->content);
}

/**
 * Sends a message to a specific client
 *
 * Checks if recipient exists and sends message or error response.
 * Logs the action to the server whiteboard.
 *
 * @param msg Message containing sender, recipient, and content
 */
void send_private_message(Message *msg)
{
    int recipient_socket = -1;

    // Check if recipient exists
    if (!find_client(msg->recipient, &recipient_socket))
    {
        // Send error back to sender
        Message error_msg;
        error_msg.type = MSG_ERROR;
        sprintf(error_msg.content, "User '%s' does not exist or is offline", msg->recipient);
        strcpy(error_msg.sender, "Server");

        int sender_socket;
        if (find_client(msg->sender, &sender_socket))
        {
            send(sender_socket, &error_msg, sizeof(Message), 0);
        }

        format_whiteboard_msg(MSG_TYPE_ERROR, "%s tried to message non-existent user %s",
                              msg->sender, msg->recipient);
        return;
    }

    // Send the private message
    send(recipient_socket, msg, sizeof(Message), 0);
    format_whiteboard_msg(MSG_TYPE_PRIVATE, "%s to %s: %s",
                          msg->sender, msg->recipient, msg->content);
}

/**
 * Thread function to manage a client connection
 *
 * Handles login verification, client registration, and message
 * processing until client disconnects or logs out.
 *
 * @param arg Pointer to client socket descriptor
 * @return Always NULL
 */
void *handle_client(void *arg)
{
    int client_socket = *((int *)arg);
    free(arg);
    Message msg;
    char username[MAX_USERNAME] = {0};

    // Receive username directly instead of waiting for a login message
    if (recv(client_socket, username, MAX_USERNAME, 0) <= 0)
    {
        close(client_socket);
        return NULL;
    }

    // Check for duplicate username
    if (find_client(username, NULL))
    {
        Message error_msg = {.type = MSG_ERROR};
        sprintf(error_msg.content, "Username '%s' is already in use", username);
        strcpy(error_msg.sender, "Server");
        send(client_socket, &error_msg, sizeof(Message), 0);
        close(client_socket);
        return NULL;
    }

    // Register the new client
    pthread_mutex_lock(&clients_mutex);
    clients[client_count].socket = client_socket;
    strncpy(clients[client_count].username, username, MAX_USERNAME);
    client_count++;
    pthread_mutex_unlock(&clients_mutex);

    format_whiteboard_msg(MSG_TYPE_LOGIN, "%s has joined the chat", username);

    // Notify others of new user
    Message login_msg = {.type = MSG_LOGIN};
    strcpy(login_msg.sender, username);
    strcpy(login_msg.content, "has joined the chat");
    broadcast_message(&login_msg, client_socket);

    // Message processing loop
    while (1)
    {
        int bytes = recv(client_socket, &msg, sizeof(Message), 0);

        if (bytes <= 0)
        {
            // Handle disconnect
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < client_count; i++)
            {
                if (clients[i].socket == client_socket)
                {
                    strcpy(username, clients[i].username);

                    // Remove client by shifting all subsequent clients
                    for (int j = i; j < client_count - 1; j++)
                    {
                        clients[j] = clients[j + 1];
                    }
                    client_count--;
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);

            format_whiteboard_msg(MSG_TYPE_LOGOUT, "%s has left the chat", username);

            Message logout_msg = {.type = MSG_LOGOUT};
            strcpy(logout_msg.sender, username);
            strcpy(logout_msg.content, "has left the chat");
            broadcast_message(&logout_msg, client_socket);
            close(client_socket);
            break;
        }
        else if (msg.type == MSG_PRIVATE)
        {
            send_private_message(&msg);
        }
    }

    return NULL;
}

/**
 * Entry point for the chat server
 *
 * Sets up server socket, initializes display, and enters
 * accept loop to handle new client connections.
 *
 * @param argc Command line argument count
 * @param argv Command line arguments (optional port)
 * @return Exit status (0 for success, 1 for errors)
 */
int main(int argc, char *argv[])
{
    // Setup port number
    int port = argc > 1 ? atoi(argv[1]) : 8888;

    // Initialize server
    printf("\n%s╔══════════════════════════════════════╗%s\n", ANSI_BOLD, ANSI_RESET);
    printf("%s║       CHAT SERVER - STARTING...     ║%s\n", ANSI_BOLD, ANSI_RESET);
    printf("%s╚══════════════════════════════════════╝%s\n\n", ANSI_BOLD, ANSI_RESET);

    update_whiteboard("SERVER STARTED");

    // Create and setup socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    // Enable address reuse
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to port
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port)};

    bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_socket, 5);

    printf("Server started on port %d\n", port);

    // Main accept loop
    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int *client_socket = malloc(sizeof(int));

        *client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);

        // Create and detach client thread
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, client_socket) == 0)
        {
            pthread_detach(thread_id);
        }
        else
        {
            close(*client_socket);
            free(client_socket);
        }
    }

    return 0;
}
