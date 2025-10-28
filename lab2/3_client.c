// The server chooses a random float number <SRF>. Run multiple clients.
// Each client chooses a random float number <CRF> and send it to the server.
// When the server does not receive any incoming connection
// for at least 10 seconds it chooses the client that has guessed the best
// approximation (is closest) for its own number and sends it back the message
// “You have the best guess with an error of <SRV>-<CRF>”.
// It also sends to each other client the string “You lost !”.
// The server closes all connections after this.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <WinSock2.h>
    #include <stdint.h>
    #define closesocket closesocke
    typedef SOCKET socket_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define closesocket close
typedef int socket_t;
#endif

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }

    socket_t sock;
    struct sockaddr_in server;
    float crf;
    char buf[128];
    int n;



#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) < 0) {
        printf("Error initializing Winsock!\n");
        return 1;
    }
#endif

    srand((unsigned int)time(NULL));
    crf = (float)rand() / (float)RAND_MAX;


    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(4321);
    server.sin_addr.s_addr = inet_addr(argv[1]);

    printf("Trying to connect to server: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("connect");
        closesocket(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    printf("Connection successful!\n");

    send(sock, (char*)&crf, sizeof(float), 0);

    n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        printf("Server: %s", buf);
    }

    closesocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
