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
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <time.h>

#define closesocket close
#define MAX_CLIENTS 100


typedef struct {
    int conn;
    float crf;
    int id;
} client_info;

client_info clients[MAX_CLIENTS];
int client_count = 0;
float srf;


void* handle_client(void* arg) {
    return NULL;
}

int main() {
    struct sockaddr_in client, server;
    int sock, code, len, conn;
    pthread_t tids[MAX_CLIENTS];

    srand(time(NULL));
    srf = (float)rand() / (float)RAND_MAX;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Error creating server socket!");
        return 1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_port = htons(4321);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;

    code = bind(sock, (struct sockaddr *) &server, sizeof(struct sockaddr_in));
    if (code < 0) {
        perror("Error binding!");
        return 1;
    }

    listen(sock, 10);

    time_t last_conn = time(NULL);

    while (1) {
        memset(&client, 0, sizeof(client));
        len = sizeof(client);

        printf("Listening for incomming connections...\n");
        conn = accept(sock, (struct sockaddr *)&client, &len);
        if (conn < 0) {
            perror("Error accept!");
            continue;
        }
        printf("Connection income: %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        last_conn = time(NULL);

        clients[client_count].conn = conn;
        clients[client_count].id = client_count;
        pthread_create(&tids[client_count], NULL, handle_client, &clients[client_count]);
        client_count++;

        if (client_count > 0 && time(NULL) - last_conn > 10) {
            printf("Timeout!");
            break;
        }
    }

    for (int i = 0; i < client_count; i++)
        pthread_join(tids[i], NULL);

    int winner = 0;
    float min_err = fabsf(srf - clients[0].crf);
    for (int i = 1; i < client_count; i++) {
        float err = fabsf(srf - clients[i].crf);
        if (err < min_err) {
            min_err = err;
            winner = i;
        }
    }

    for (int i = 0; i < client_count; i++) {
        if (i == winner) {
            char msg[128];
            snprintf(msg, sizeof(msg), "You have the best guess with an error of %f\n", min_err);
            write(clients[i].conn, msg, strlen(msg));
        } else {
            write(clients[i].conn, "You lost !\n", 10);
        }
        close(clients[i].conn);
    }
    close(sock);
    return 0;
}
