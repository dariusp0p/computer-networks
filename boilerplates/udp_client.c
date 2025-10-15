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


int main(int argc, char* argv[]) {
    SOCKET sock;
    struct sockaddr_in server;
    int addr_len = sizeof(server);

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) < 0) {
        printf("Error initializing the Windows Sockets Library!\n");
        return 1;
    }
#endif

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("Error creating client socket!\n");
        return 1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_port = htons(1234);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[2]);

    sendto(sock, date_str, strlen(date_str) + 1, 0, (struct sockaddr*)&server, addr_len);

    int res = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&server, &addr_len);
    if (res > 0) {}

    closesocket(sock);

#ifdef _WIN32
    WSACleanup();
#endif
}
