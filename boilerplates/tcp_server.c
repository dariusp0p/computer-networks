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
    int cl, len, err;

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) < 0) {
        printf("Error initializing the Windows Sockets Library!\n");
        return 1;
    }
#endif

    sock = socket(AF_INET, SOCK_STREAM, 0);
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

    listen(sock, 5);
    len = sizeof(client);
    memset(&client, 0, sizeof(client));

    while (1) {
      char buffer[1024];

      printf("Listening for incomming connections...\n");
      cl = accept(sock, (struct sockaddr *) &client, &len);
      err = errno;
#ifdef _WIN32
      err = WSAGetLastError();
#endif
      if (cl < 0) {
          printf("Error accept: %d\n", err);
          continue;
      }
      printf("Connected client: %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

      int res = recv(cl, (char*) &buffer, sizeof(buffer), 0);
      if (res != sizeof(buffer)) printf("Error receiving string!\n");

      send(cl, &buffer, sizeof(buffer), 0);

      closesocket(cl);
   }
#ifdef _WIN32
    WSACleanup();
#endif
}
