#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #define closesocket close
  typedef int SOCKET;
#else
#include <WinSock2.h>
#endif

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("Socket error\n");
        return 1;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(1234);
    server.sin_addr.s_addr = inet_addr("192.168.1.11");

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        printf("Connect error\n");
        closesocket(sock);
        return 1;
    }

    int arr_size;
    printf("Enter array size: ");
    scanf("%d", &arr_size);

    int *arr = (int*)malloc(arr_size * sizeof(int));
    if (!arr) {
        printf("Memory error\n");
        closesocket(sock);
        return 1;
    }

    printf("Enter %d integers:\n", arr_size);
    for (int i = 0; i < arr_size; ++i) {
        scanf("%d", &arr[i]);
        arr[i] = htonl(arr[i]);
    }

    int arr_size_net = htonl(arr_size);
    send(sock, (char*)&arr_size_net, sizeof(arr_size_net), 0);
    send(sock, (char*)arr, arr_size * sizeof(int), 0);

    int sum_net, res;
    res = recv(sock, (char*)&sum_net, sizeof(sum_net), 0);
    if (res == sizeof(sum_net)) {
        printf("Sum: %d\n", ntohl(sum_net));
    } else {
        printf("Error receiving sum\n");
    }

    free(arr);
    closesocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}