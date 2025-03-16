# Client-Server Chat Application

A multi-threaded chat application using Unix sockets with support for private messaging.

## Features

- Multiple client support
- Private messaging between specific clients
- Real-time notifications for user connections/disconnections
- Thread-safe whiteboard logging of recent messages

## Building the Application

### Prerequisites
- GCC compiler
- POSIX threads library
- Make utility

### Compilation
Build both server and client applications:
```bash
make
```

Clean compiled binaries:
```bash
make clean
```

## Executing the Program

### Starting the Server
```bash
./server [port]
```
- Default port is 8888 if not specified

### Starting a Client
```bash
./client <username> [port]
```
- `username`: Required - your display name (max 20 chars)
- `port`: Optional - must match server port (default: 8888)

### Usage Examples
```bash
# Start server on default port 8888
./server

# Start server on custom port 9000
./server 9000

# Connect client on default port
./client alice

# Connect client on custom port
./client bob 9000
```

### Client Commands
- Send private message: `<recipient> <message>`
- Example: `bob Hello, how are you?`

## Program Maintenance

### Code Structure
- `common.h` - Shared definitions and constants
- `server.c` - Server implementation with client handling logic
- `client.c` - Client implementation with UI and messaging logic

### Key Components

#### Server Components
- Client management - Adding/removing clients in the client list
- Message broadcasting - Sending messages to all or specific clients
- Whiteboard system - Server-side display of activity

#### Client Components  
- Message receiving thread - Handles incoming messages
- User input parsing - Processes commands and sends messages
- Display formatting - Formats messages with color codes

### Modifying the Program

#### Adding New Message Types
1. Add new type to `MessageType` enum in `common.h`
2. Add handler in `handle_client()` function in server.c
3. Add display logic in `receive_messages()` function in client.c

#### Changing Display Format
- Modify `format_whiteboard_msg()` in server.c for server display
- Modify `print_message()` in client.c for client display

#### Extending Client Capacity
- Modify `MAX_CLIENTS` in common.h (default: 10)
