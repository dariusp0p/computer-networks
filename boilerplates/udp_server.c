#define _WINSOCK_DEPRECATED_NO_WARNINGS 1

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/ip.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>
    #define closesocket close
    typedef int SOCKET;
#else
    #include <WinSock2.h>
    #include <stdint.h>
#endif


int main() {
    SOCKET sock;
    struct sockaddr_in server, client;
    int len = sizeof(client);
    int cl, err;

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) < 0) {
        printf("Error initializing the Windows Sockets Library!\n");
        return 1;
    }
#endif

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("Error creating server socket!\n");
        return 1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_port = htons(1234);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
        perror("Error binding!\n");
        return 1;
    }

    while (1) {
        char buffer[1024];
        int res = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&client, &len);
        if (res < 0) {
            perror("Error receiving!\n");
            continue;
        }
        sendto(sock, day, strlen(day) + 1, 0, (struct sockaddr*)&client, len);
    }
    closesocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
}
