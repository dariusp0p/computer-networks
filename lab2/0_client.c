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

#define MAX 100


int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }

    SOCKET c;
    int code;
    int32_t result;
    struct sockaddr_in server;
    char string[MAX];

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) < 0) {
        fprintf(stderr, "Error initializing the Windows Sockets Library!\n");
        return 1;
    }
#endif

    c = socket(PF_INET, SOCK_STREAM, 0);
    if (c < 0) {
    #ifdef _WIN32
        int err = WSAGetLastError();
        fprintf(stderr, "Error creating client socket: %d\n", err);
    #else
        perror("Error creating client socket");
    #endif
        return 1;
    }

    memset(&server, 0, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(4321);
    server.sin_addr.s_addr = inet_addr(argv[1]);

    code = connect(c, (struct sockaddr *) &server, sizeof(struct sockaddr_in));
    if (code < 0) {
    #ifdef _WIN32
        int err = WSAGetLastError();
        fprintf(stderr, "Error connecting to server: %d\n", err);
    #else
        perror("Error connecting to server");
    #endif
        closesocket(c);
        return 1;
    }

    printf("Enter a sentence to send to the server: ");
    fgets(string, MAX, stdin);

    code = send(c, string, strlen(string) + 1, 0);
    if (code != strlen(string) + 1) {
    #ifdef _WIN32
        int err = WSAGetLastError();
        fprintf(stderr, "Error sending data to server: %d\n", err);
    #else
        perror("Error sending data to server");
    #endif
        closesocket(c);
        return 1;
    }

    code = recv(c, &result, sizeof(int32_t), MSG_WAITALL);
    if (code != sizeof(int32_t)) {
    #ifdef _WIN32
        int err = WSAGetLastError();
        fprintf(stderr, "Error receiving data from server: %d\n", err);
    #else
        perror("Error receiving data from server");
    #endif
        closesocket(c);
        return 1;
    }

    result = ntohl(result);
    printf("The server returned %d space characters in the sent string.\n", result);

    closesocket(c);

#ifdef _WIN32
    WSACleanup();
#endif
}