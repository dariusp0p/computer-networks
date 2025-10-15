#define _WINSOCK_DEPRECATED_NO_WARNINGS 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifndef _WIN32
    #include <unistd.h>
    #include <signal.h>
    #include <errno.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/ip.h>
    #include <arpa/inet.h>
    #define closesocket close
    typedef int SOCKET;
#else
    #include <windows.h>
    #include <WinSock2.h>
#endif


int conn;


void time_out(int semnal) {
    int32_t result = -1;
    result = htonl(result);
    printf("Time out.\n");
    send(conn, &result, sizeof(int32_t), 0);
    close(conn);
    exit(1);
}
 
void handle_client() {
    int32_t result;
    uint8_t buffer;
    struct sockaddr_in server;
    int code;

    if (conn < 0) {
    #ifdef _WIN32
        int err = WSAGetLastError();
        fprintf(stderr, "Error establishing connection with client: %d\n", err);
    #else
        perror("Error establishing connection with client!");
    #endif
        exit(1);
    } else {
        printf("The client has successfully connected!\n");
    }

    signal(SIGALRM, time_out);
    alarm(10);

    do {
        code = recv(conn, &buffer, 1, 0);
        printf("%d characters recieved.\n", code);
        if (code == 1)
            alarm(10);
        if (code != 1) {
            result = -1;
            break;
        }
        if (buffer == ' ') {
            if (result == INT32_MAX) {
                result = -2;
                break;
            }
            result++;
        }
    } while (buffer != 0);

    alarm(0);

    result = htonl(result);
    send(conn, &result, sizeof(int32_t), 0);
    result = ntohl(result);

    closesocket(conn);

    if (result >= 0) {
        printf("Successfully closed the connection with a client. Sent %d spaces.\n", result);
    } else {
        printf("Closed the connection with a client due to an error. Error code: %d.\n", result);
        exit(1);
    }
    exit(0);
}
 

int main() {
    SOCKET sock;
    struct sockaddr_in client, server;
    int code, len;

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) < 0) {
        perror("Error initializing the Windows Sockets Library!\n");
        return 1;
    }
#endif

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
    #ifdef _WIN32
        int err = WSAGetLastError();
        fprintf(stderr, "Error creating server socket: %d\n", err);
    #else
        perror("Error creating server socket!");
    #endif
        return 1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_port = htons(4321);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;

    code = bind(sock, (struct sockaddr *) &server, sizeof(struct sockaddr_in));
    if (code < 0) {
    #ifdef _WIN32
        int err = WSAGetLastError();
        fprintf(stderr, "Error binding: %d\n", err);
    #else
        perror("Error binding!");
    #endif
        return 1;
    }

    listen(sock, 5);

    while (1) {
        memset(&client, 0, sizeof(client));
        len = sizeof(client);

        printf("Listening for incomming connections...\n");
        conn = accept(sock, (struct sockaddr *)&client, &len);
        if (conn < 0) {
        #ifdef _WIN32
            int err = WSAGetLastError();
            fprintf(stderr, "Error accept: %d\n", err);
        #else
            perror("Error accept!");
        #endif
            continue;
        }
        printf("Connection income: %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        if (fork() == 0) {
            handle_client();
        }
    }
#ifdef _WIN32
    WSACleanup();
#endif
}