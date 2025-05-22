// WSEchoServerv6.c

// Include your existing headers
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Ws2ipdef.h>

// Add the Rust library header
#include "echo_server_threads.h"

#define DEFAULT_PORT 5000
#define MAXQUEUED 5
#define NUM_THREADS 8  // Number of threads in the pool

// External function declarations
extern void DisplayFatalErr(char* errMsg);

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    SOCKET serverSock;
    struct sockaddr_in6 serverInfo;
    unsigned short serverPort = DEFAULT_PORT;

    // Initialize the WinSock DLL for version 2.0
    if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) {
        DisplayFatalErr("WSAStartup() failed.");
        exit(1);
    }

    // Create a TCP socket using IPv6
    serverSock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (serverSock == INVALID_SOCKET) {
        DisplayFatalErr("socket() failed to create socket.");
    }

    // Get the server port number from the command line or use default port
    if (argc == 2) {
        serverPort = (unsigned short)atoi(argv[1]);
    }
    else {
        printf("No port provided, using the default port %d\n", DEFAULT_PORT);
    }

    // Initialize the server address sockaddr_in6 structure
    memset(&serverInfo, 0, sizeof(serverInfo));
    serverInfo.sin6_family = AF_INET6;
    serverInfo.sin6_port = htons(serverPort);
    serverInfo.sin6_addr = in6addr_any;

    // Bind the server socket
    if (bind(serverSock, (struct sockaddr*)&serverInfo, sizeof(serverInfo)) == SOCKET_ERROR) {
        DisplayFatalErr("bind() failed to bind server socket.");
    }

    // Set to listen
    if (listen(serverSock, MAXQUEUED) == SOCKET_ERROR) {
        DisplayFatalErr("listen() failed to set server socket to listen.");
    }

    // Initialize the Rust thread pool
    if (!init_thread_pool(NUM_THREADS)) {
        DisplayFatalErr("Failed to initialize thread pool.");
    }

    printf("RC's IPv6 echo server with Rust thread pool is ready on port %d...\n", serverPort);

    // Forever loop to accept connections
    for (;;) {
        struct sockaddr_in6 clientInfo;
        int clientInfoLen = sizeof(clientInfo);

        // Accept a connection
        SOCKET clientSock = accept(serverSock, (struct sockaddr*)&clientInfo, &clientInfoLen);
        if (clientSock == INVALID_SOCKET) {
            DisplayFatalErr("accept() failed to accept client connection.");
        }

        // Log the connection
        char clientIP[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &clientInfo.sin6_addr, clientIP, sizeof(clientIP));
        int clientPort = ntohs(clientInfo.sin6_port);
        printf("Connection from %s, port %d\n", clientIP, clientPort);

        // Hand off to Rust thread pool
        if (!process_client_threaded((int)clientSock)) {
            // If processing failed, close the socket
            closesocket(clientSock);
            fprintf(stderr, "Failed to process client in thread pool\n");
        } else {
            // Important: do NOT close the socket here, as Rust has taken ownership of it
            printf("Client handed off to thread pool\n");
        }
    }

    // This part would only be reached if you added a way to break out of the for loop
    shutdown_thread_pool();
    closesocket(serverSock);
    WSACleanup();

    return 0;
}
