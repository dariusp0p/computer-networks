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
#include <fcntl.h>
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

pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t timer_cond = PTHREAD_COND_INITIALIZER;
int timeout = 0;


void* timer_thread_func(void* arg) {
    while (1) {
        pthread_mutex_lock(&timer_mutex);
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 10;
        int rc = pthread_cond_timedwait(&timer_cond, &timer_mutex, &ts);
        if (rc == ETIMEDOUT) {
            timeout = 1;
            pthread_mutex_unlock(&timer_mutex);
            break;
        }
        pthread_mutex_unlock(&timer_mutex);
    }
    return NULL;
}

void* handle_client(void* arg) {
    client_info* info = arg;
    float crf;
    int n = read(info->conn, &crf, sizeof(float));
    if (n == sizeof(float)) {
        info->crf = crf;
    }
    printf("Player %d guess is: %f\n", info->id, crf);
    return NULL;
}


int main() {
    struct sockaddr_in client, server;
    int sock, code, len, conn;
    pthread_t tids[MAX_CLIENTS];
    pthread_t timer_thread;

    srand(time(NULL));
    srf = (float)rand() / (float)RAND_MAX;
    printf("The number is %f\n", srf);

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Error creating server socket!");
        return 1;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

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

    pthread_create(&timer_thread, NULL, timer_thread_func, NULL);

    while (!timeout) {
        memset(&client, 0, sizeof(client));
        len = sizeof(client);

        printf("Listening for incomming connections...\n");
        conn = accept(sock, (struct sockaddr *)&client, &len);
        if (conn < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000000);
                continue;
            }
            perror("Error accept!");
            continue;
        }
        printf("New player from: %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        pthread_mutex_lock(&timer_mutex);
        pthread_cond_signal(&timer_cond);
        pthread_mutex_unlock(&timer_mutex);

        clients[client_count].conn = conn;
        clients[client_count].id = client_count;
        pthread_create(&tids[client_count], NULL, handle_client, &clients[client_count]);
        client_count++;
    }

    pthread_join(timer_thread, NULL);
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
            snprintf(msg, sizeof(msg), "You won! The number was %f\n", srf);
            write(clients[i].conn, msg, strlen(msg));
        } else {
            char msg[128];
            snprintf(msg, sizeof(msg), "You lost! The number was %f\n", srf);
            write(clients[i].conn, msg, strlen(msg));
        }
        close(clients[i].conn);
    }
    close(sock);
    return 0;
}
