#define _WINSOCK_DEPRECATED_NO_WARNINGS 1

#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/ip.h>
  #include <string.h>
  #include <arpa/inet.h>

  #include <unistd.h>
  #include <errno.h>

  #define closesocket close
  typedef int SOCKET;
#else
  #include<WinSock2.h>
  #include<stdint.h>
#endif

int main() {
  SOCKET sock;
  struct sockaddr_in server, client;
  int cl, len, err;

#ifdef _WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) < 0) {
    printf("Error initializing the Windows Sockets LIbrary");
    return -1;
  }
#endif

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    printf("Error initializing the Windows Sockets LIbrary\n");
    return 1;
  }

  memset(&server, 0, sizeof(server));
  server.sin_port = htons(1234);
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
    perror("Bind error:");
    return 1;
  }

  listen(sock, 5);

  len = sizeof(client);
  memset(&client, 0, sizeof(client));

  while (1) {
    int arr_size, res;
    printf("Listening for incomming connections...\n");
    cl = accept(sock, (struct sockaddr *) &client, &len);
    err = errno;
#ifdef _WIN32
    err = WSAGetLastError();
#endif
    if (cl < 0) {
      printf("accept error: %d", err);
      continue;
    }

    printf("Incomming connected client from: %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

    res = recv(cl, (char*)&arr_size, sizeof(arr_size), 0);
    if (res != sizeof(arr_size)) {
      printf("Error receiving array size\n");
      closesocket(cl);
      continue;
    }
    arr_size = ntohl(arr_size);

    int *arr = (int*)malloc(arr_size * sizeof(int));
    if (!arr) {
      printf("Memory allocation error\n");
      closesocket(cl);
      continue;
    }

    res = recv(cl, (char*)arr, arr_size * sizeof(int), 0);
    if (res != arr_size * sizeof(int)) {
      printf("Error receiving array\n");
      free(arr);
      closesocket(cl);
      continue;
    }

    int sum = 0;
    for (int i = 0; i < arr_size; ++i) {
      arr[i] = ntohl(arr[i]);
      sum += arr[i];
    }
    free(arr);

    int sum_net = htonl(sum);
    res = send(cl, (char*)&sum_net, sizeof(sum_net), 0);
    if (res != sizeof(sum_net)) printf("Error sending result\n");
    closesocket(cl);
  }
#ifdef _WIN32
  WSACleanup();
#endif
}