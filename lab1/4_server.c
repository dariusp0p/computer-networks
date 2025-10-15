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
        char buffer1[32], buffer2[32], result[64];
        int res, rec;
        uint32_t size;

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

        recv(cl, &size, sizeof(size), 0);
        if (size >= sizeof(buffer1)) size = sizeof(buffer1) - 1;
        rec = 0;
        while (rec < size) {
            res = recv(cl, buffer1 + rec, size - rec, 0);
            if (res <= 0) break;
            rec += res;
        }
        buffer1[size] = '\0';

        recv(cl, &size, sizeof(size), 0);
        if (size >= sizeof(buffer1)) size = sizeof(buffer1) - 1;
        rec = 0;
        while (rec < size) {
            res = recv(cl, buffer2 + rec, size - rec, 0);
            if (res <= 0) break;
            rec += res;
        }
        buffer2[size] = '\0';

        int i = 0, j = 0, k = 0;
        while (i < strlen(buffer1) && j < strlen(buffer2) && k < sizeof(result) - 1) {
            if (buffer1[i] <= buffer2[j]) {
                result[k++] = buffer1[i++];
            } else {
                result[k++] = buffer2[j++];
            }
        }
        while (i < strlen(buffer1) && k < sizeof(result) - 1)
            result[k++] = buffer1[i++];
        while (j < strlen(buffer2) && k < sizeof(result) - 1)
            result[k++] = buffer2[j++];
        result[k] = '\0';

        size = strlen(result);
        send(cl, &size, sizeof(size), 0);
        send(cl, result, size, 0);

        closesocket(cl);
    }
#ifdef _WIN32
    WSACleanup();
#endif
}
