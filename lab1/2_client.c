// 2.A client sends to the server a string. The server returns the count of spaces in the string.

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
    struct sockaddr_in server;
    char buffer[256];
    uint8_t spaces;

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) < 0) {
        printf("Error initializing the Windows Sockets Library!\n");
        return 1;
    }
#endif

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("Error creating client socket!\n");
        return 1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_port = htons(1234);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("192.168.1.10");

    printf("Trying to connect to server: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
    if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
        printf("Server connection error!\n");
        return 1;
    }
    printf("Connection successfull!\n");

    printf("Enter a string: ");
    fgets(buffer, sizeof(buffer), stdin);

    send(sock, buffer, sizeof(buffer), 0);
    recv(sock, &spaces, sizeof(spaces), 0);

    printf("The string has %hhu spaces!\n", spaces);

    closesocket(sock);



#ifdef _WIN32
    WSACleanup();
#endif
}
