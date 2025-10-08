// 4. The client sends to the server two sorted array of chars.
// The server will merge sort the two arrays and return the result to the client.

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
    char arr1[32], arr2[32], result[64];
    uint32_t size;

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
    server.sin_addr.s_addr = inet_addr("192.168.1.10"); // dont't forget

    printf("Trying to connect to server: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
    if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
        printf("Server connection error!\n");
        return 1;
    }
    printf("Connection successfull!\n");

    FILE* fd = fopen("input_4.txt", "r");
    fscanf(fd, "%31s %31s", arr1, arr2);
    fclose(fd);

    size = strlen(arr1);
    send(sock, &size, sizeof(size), 0);
    send(sock, arr1, size, 0);
    size = strlen(arr2);
    send(sock, &size, sizeof(size), 0);
    send(sock, arr2, size, 0);

    recv(sock, &size, sizeof(size), 0);
    recv(sock, result, size, 0);

    printf("Result is: %63s", result);

    closesocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
}
