#include "common.h"
#include <errno.h>
#include <stdarg.h> // Add this header for va_start, va_end

int sock = -1;
char username[MAX_USERNAME];
pthread_t recv_thread;
int connected = 0;

/**
 * Print formatted messages with optional timestamp
 *
 * Handles error messages and normal messages with appropriate color formatting.
 *
 * @param type 0 for normal message, 1 for error message
 * @param format Printf-style format string
 * @param ... Variable arguments to be formatted
 */
void print_message(int type, const char *format, ...)
{
    if (type == 1)
    {
        // Error message with red formatting
        printf("%s[!] ", ANSI_RED);
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    if (type == 1)
    {
        printf("%s\n", ANSI_RESET);
    }
    else
    {
        printf("\n");
    }
}

/**
 * Thread function to receive messages from the server
 *
 * Runs in separate thread and handles different message types with
 * appropriate formatting. Terminates when connection closes.
 *
 * @param arg Thread argument (not used)
 * @return NULL when thread terminates
 */
void *receive_messages(void *arg)
{
    Message msg;

    while (connected)
    {
        int bytes_received = recv(sock, &msg, sizeof(Message), 0);
        if (bytes_received <= 0)
        {
            printf("\n%s[!] Server disconnected%s\n> ", ANSI_RED, ANSI_RESET);
            fflush(stdout);
            connected = 0;
            break;
        }

        printf("\n"); // Prevent overwriting the prompt

        switch (msg.type)
        {
        case MSG_BROADCAST:
            print_message(0, "%s%s%s: %s",
                          ANSI_BOLD, msg.sender, ANSI_RESET, msg.content);
            break;
        case MSG_PRIVATE:
            print_message(0, "%s%s%s %s%s→%s %s: %s%s",
                          ANSI_BOLD, ANSI_MAGENTA, msg.sender, ANSI_BLUE, ANSI_BOLD,
                          ANSI_RESET, ANSI_MAGENTA, msg.content, ANSI_RESET);
            break;
        case MSG_LOGIN:
        case MSG_LOGOUT:
            print_message(0, "%s*** %s %s ***%s",
                          msg.type == MSG_LOGIN ? ANSI_GREEN : ANSI_YELLOW,
                          msg.sender, msg.content, ANSI_RESET);
            break;
        case MSG_ERROR:
            print_message(0, "%sError: %s%s",
                          ANSI_RED, msg.content, ANSI_RESET);
            break;
        }

        printf("> ");
        fflush(stdout);
    }
    return NULL;
}

/**
 * Send a private message to a recipient
 *
 * Builds a Message structure with sender, recipient and content,
 * then sends it to the server.
 *
 * @param recipient Username of recipient
 * @param content Message content
 */
void send_message(const char *recipient, const char *content)
{
    if (!connected)
    {
        printf("%s[!] Not connected to server%s\n", ANSI_RED, ANSI_RESET);
        return;
    }

    Message msg;
    msg.type = MSG_PRIVATE;
    strncpy(msg.sender, username, MAX_USERNAME);
    strncpy(msg.recipient, recipient, MAX_USERNAME);
    strncpy(msg.content, content, MAX_MESSAGE);

    send(sock, &msg, sizeof(Message), 0);
}

/**
 * Connect to the chat server and set up message receiving thread
 *
 * Creates socket, connects to server, sets connected flag,
 * sends username information, and creates receive thread.
 *
 * @param server_port Port number to connect to
 */
void connect_to_server(int server_port)
{
    const char *server_ip = "127.0.0.1"; // Local server

    // Create socket and connect
    sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(server_port)};

    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("%s[!] Connection failed%s\n", ANSI_RED, ANSI_RESET);
        exit(1);
    }

    connected = 1;

    // Send username to server (server will handle the login announcement)
    write(sock, username, MAX_USERNAME);

    // Create message receiver thread
    pthread_create(&recv_thread, NULL, receive_messages, NULL);
}

/**
 * Entry point of the chat client
 *
 * Processes command-line args, connects to server, and handles
 * user input loop for commands and messages until exit.
 *
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return 0 on success, 1 if arguments are invalid
 */
int main(int argc, char *argv[])
{
    int server_port = 8888; // Default port

    // Verify arguments and set username/port
    if (argc < 2)
    {
        printf("Usage: %s <username> [port]\n", argv[0]);
        return 1;
    }

    strncpy(username, argv[1], MAX_USERNAME - 1);
    username[MAX_USERNAME - 1] = '\0';

    if (argc >= 3)
    {
        server_port = atoi(argv[2]);
        if (server_port <= 0)
            server_port = 8888;
    }

    // Display welcome message
    printf("\n%s╔══════════════════════════════════════╗%s\n", ANSI_BOLD, ANSI_RESET);
    printf("%s║       PRIVATE CHAT CLIENT            ║%s\n", ANSI_BOLD, ANSI_RESET);
    printf("%s╚══════════════════════════════════════╝%s\n\n", ANSI_BOLD, ANSI_RESET);
    printf("Welcome, %s%s%s!\n", ANSI_BOLD, username, ANSI_RESET);
    printf("To send a message, type: %s<username> <message>%s\n", ANSI_BOLD, ANSI_RESET);

    connect_to_server(server_port);

    // Main input loop
    char input[MAX_MESSAGE];
    while (1)
    {
        printf("> ");
        if (!fgets(input, MAX_MESSAGE, stdin) || !connected)
            break;

        // Remove newline character
        input[strcspn(input, "\n")] = 0;
        if (strlen(input) == 0)
            continue;

        // Handle message sending: <recipient> <message>
        char *space = strchr(input, ' ');
        if (!space)
        {
            printf("%s[!] Usage: <username> <message>%s\n", ANSI_YELLOW, ANSI_RESET);
            continue;
        }

        // Extract recipient and message
        int name_len = space - input;
        if (name_len >= MAX_USERNAME)
            name_len = MAX_USERNAME - 1;

        char recipient[MAX_USERNAME];
        strncpy(recipient, input, name_len);
        recipient[name_len] = '\0';

        char *message = space + 1;
        send_message(recipient, message);
    }

    return 0;
}
