// 5. The client sends to the server an integer. The server returns the list of divisors for the specified number.

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

    int x;

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
    server.sin_addr.s_addr = inet_addr("172.30.244.221"); // dont't forget

    printf("Trying to connect to server: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
    if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
        printf("Server connection error!\n");
        return 1;
    }
    printf("Connection successfull!\n");

    printf("x = ");
    scanf("%d", &x);
    printf("The divisors of %d are: ", x);


    int buffer = htonl(x);
    send(sock, &buffer, sizeof(buffer), 0);

    int res = recv(sock, (char*)&buffer, sizeof(buffer), 0);
    if (res != sizeof(buffer)) printf("Error receiving nr of divisors!\n");
    buffer = ntohl(buffer);

    for (int i = 0; i < buffer; i++) {
        int buffer2;
        int res = recv(sock, (char*)&buffer2, sizeof(buffer2), 0);
        if (res != sizeof(buffer2)) printf("Error receiving %dth divisor!\n", i);
        buffer2 = ntohl(buffer2);
        printf("%d, ", buffer2);
    }

    closesocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
}
