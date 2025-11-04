// 4. Implement the Chat server example (see the link bellow) using UDP and TCP
// –only this time each client should contact the server just for registration.
// All communication happens directly between the peers (clients) without
// passing trough the server. Each client maintains an endpoint (TCP/UDP) with
// the server just for learning the arrival/departure of other clients. You
// create a mesh architecture where all clients connect directly between each
// others.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>

#pragma comment(lib, "Ws2_32.lib")

#define BUFSIZE 256

static unsigned __stdcall stdin_thread(void *arg) {
    SOCKET sock = *(SOCKET *)arg;
    char buf[BUFSIZE];
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        size_t len = strlen(buf);
        if (len == 0) continue;
        int sent = send(sock, buf, (int)len, 0);
        if (sent == SOCKET_ERROR) {
            fprintf(stderr, "send failed: %d\n", WSAGetLastError());
            break;
        }
    }
    // Signal EOF to peer and exit thread
    shutdown(sock, SD_SEND);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s <hostname or IP> <port>\n", argv[0]);
        return 1;
    }

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(argv[1], argv[2], &hints, &res) != 0) {
        fprintf(stderr, "getaddrinfo failed\n");
        WSACleanup();
        return 1;
    }

    SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }

    if (connect(sock, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        fprintf(stderr, "connect failed: %d\n", WSAGetLastError());
        closesocket(sock);
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(res);

    // Start stdin reader thread
    uintptr_t th = _beginthreadex(NULL, 0, stdin_thread, &sock, 0, NULL);
    if (th == 0) {
        fprintf(stderr, "Failed to create thread\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Main thread receives and writes to stdout
    char buf[BUFSIZE];
    int nbytes;
    while ((nbytes = recv(sock, buf, sizeof(buf), 0)) > 0) {
        fwrite(buf, 1, nbytes, stdout);
        fflush(stdout);
    }

    if (nbytes == 0) {
        printf("Server closed connection\n");
    } else {
        fprintf(stderr, "recv failed: %d\n", WSAGetLastError());
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}