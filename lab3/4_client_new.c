// 4. Implement the Chat server example (see the link bellow) using UDP and TCP
// –only this time each client should contact the server just for registration.
// All communication happens directly between the peers (clients) without
// passing trough the server. Each client maintains an endpoint (TCP/UDP) with
// the server just for learning the arrival/departure of other clients. You
// create a mesh architecture where all clients connect directly between each
// others.

#define _WINSOCK_DEPRECATED_NO_WARNINGS 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    if (server_port <= 0 || server_port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[2]);
        return 1;
    }

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons((unsigned short)server_port);

    if (inet_pton(AF_INET, server_ip, &server.sin_addr) != 1) {
        server.sin_addr.s_addr = inet_addr(server_ip);
        if (server.sin_addr.s_addr == INADDR_NONE) {
            fprintf(stderr, "Invalid server IP: %s\n", server_ip);
            closesocket(sock);
            WSACleanup();
            return 1;
        }
    }

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        fprintf(stderr, "connect failed: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    struct sockaddr_in local;
    int local_len = sizeof(local);
    if (getsockname(sock, (struct sockaddr *)&local, &local_len) == SOCKET_ERROR) {
        fprintf(stderr, "getsockname failed: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    char local_ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &local.sin_addr, local_ip, sizeof(local_ip)) == NULL) {
        /* fallback to inet_ntoa */
        strcpy(local_ip, inet_ntoa(local.sin_addr));
    }
    int local_port = ntohs(local.sin_port);

    char sendbuf[256];
    int sendlen = snprintf(sendbuf, sizeof(sendbuf), "REGISTER %s:%d\n", local_ip, local_port);
    if (sendlen < 0) sendlen = (int)strlen(sendbuf);

    if (send(sock, sendbuf, sendlen, 0) == SOCKET_ERROR) {
        fprintf(stderr, "send failed: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    char recvbuf[512];
    int n;
    while ((n = recv(sock, recvbuf, sizeof(recvbuf) - 1, 0)) > 0) {
        recvbuf[n] = '\0';
        printf("%s", recvbuf);
        if (strchr(recvbuf, '\n')) break;
    }
    if (n == SOCKET_ERROR) {
        fprintf(stderr, "recv failed: %d\n", WSAGetLastError());
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}